/*
 *  OpenSCAD (www.openscad.org)
 *  Copyright (C) 2009-2011 Clifford Wolf <clifford@clifford.at> and
 *                          Marius Kintel <marius@kintel.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  As a special exception, you have permission to link this program
 *  with the CGAL library and distribute executables, as long as you
 *  follow the requirements of the GNU GPL in regard to all of the
 *  software in the executable aside from CGAL.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "core/Value.h"

#include <filesystem>
#include <cmath>
#include <variant>
#include <limits>
#include <ostream>
#include <utility>
#include <cstdint>
#include <cassert>
#include <cstddef>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include <boost/lexical_cast.hpp>

#include "core/EvaluationSession.h"
#include "io/fileutils.h"
#include "utils/printutils.h"
#include "utils/StackCheck.h"
#include "utils/boost-utils.h"
#include <double-conversion/double-conversion.h>
#include <double-conversion/utils.h>
#include <double-conversion/ieee.h>

namespace fs = std::filesystem;

const Value Value::undefined;
const VectorType VectorType::EMPTY(nullptr);
const RangeType RangeType::EMPTY{0, 0, 0};


/* Define values for double-conversion library. */
#define DC_BUFFER_SIZE (128)
#define DC_FLAGS (double_conversion::DoubleToStringConverter::UNIQUE_ZERO | double_conversion::DoubleToStringConverter::EMIT_POSITIVE_EXPONENT_SIGN)
#define DC_INF "inf"
#define DC_NAN "nan"
#define DC_EXP 'e'
#define DC_DECIMAL_LOW_EXP (-6)
#define DC_DECIMAL_HIGH_EXP (21)
#define DC_MAX_LEADING_ZEROES (5)
#define DC_MAX_TRAILING_ZEROES (0)

/* WARNING: using values > 8 will significantly slow double to string
 * conversion, defeating the purpose of using double-conversion library */
#define DC_PRECISION_REQUESTED (6)

//private definitions used by trimTrailingZeroesHelper
#define TRIM_TRAILINGZEROES_DONE (0)
#define TRIM_TRAILINGZEROES_CONTINUE (1)

//process parameter buffer from the end to start to find out where the zeroes are located (if any).
//parameter pos shall be the pos in buffer where '\0' is located.
//parameter currentpos shall be set to end of buffer (where '\0' is located).
//set parameters exppos and decimalpos when needed.
//leave parameter zeropos as is.
inline int trimTrailingZeroesHelper(char *buffer, const int pos, char *currentpos = nullptr, char *exppos = nullptr, char *decimalpos = nullptr, char *zeropos = nullptr) {

  int cont = TRIM_TRAILINGZEROES_CONTINUE;

  //we have exhausted all positions from end to start
  if (currentpos <= buffer) return TRIM_TRAILINGZEROES_DONE;

  //we do no need to process the terminator of string
  if (*currentpos == '\0') {
    currentpos--;
    cont = trimTrailingZeroesHelper(buffer, pos, currentpos, exppos, decimalpos, zeropos);
  }

  //we have an exponent and jumps to the position before the exponent - no need to process the characters belonging to the exponent
  if (cont && exppos && currentpos >= exppos) {
    currentpos = exppos;
    currentpos--;
    cont = trimTrailingZeroesHelper(buffer, pos, currentpos, exppos, decimalpos, zeropos);
  }

  //we are still on the right side of the decimal and still counting zeroes (keep track of) from the back to start
  if (cont && currentpos && decimalpos < currentpos && *currentpos == '0') {
    zeropos = currentpos;
    currentpos--;
    cont = trimTrailingZeroesHelper(buffer, pos, currentpos, exppos, decimalpos, zeropos);
  }

  //we have found the first occurrence of not a zero and have zeroes and exponent to take care of (move exponent to either the position of the zero or the decimal)
  if (cont && zeropos && exppos) {
    int count = &buffer[pos] - exppos + 1;
    memmove(zeropos - 1 == decimalpos ? decimalpos : zeropos, exppos, count);
    return TRIM_TRAILINGZEROES_DONE;
  }

  //we have found a zero and need to take care of (truncate the string to the position of either the zero or the decimal)
  if (cont && zeropos) {
    zeropos - 1 == decimalpos ? *decimalpos = '\0' : *zeropos = '\0';
    return TRIM_TRAILINGZEROES_DONE;
  }

  //we have just another character (other than a zero) and are done
  if (cont && !zeropos) return TRIM_TRAILINGZEROES_DONE;

  return TRIM_TRAILINGZEROES_DONE;
}

inline void trimTrailingZeroes(char *buffer, const int pos) {
  char *decimal = strchr(buffer, '.');
  if (decimal) {
    char *exppos = strchr(buffer, DC_EXP);
    trimTrailingZeroesHelper(buffer, pos, &buffer[pos], exppos, decimal, nullptr);
  }
}

inline bool HandleSpecialValues(const double& value, double_conversion::StringBuilder& builder) {
  double_conversion::Double double_inspect(value);
  if (double_inspect.IsInfinite()) {
    if (value < 0) {
      builder.AddCharacter('-');
    }
    builder.AddString(DC_INF);
    return true;
  }
  if (double_inspect.IsNan()) {
    builder.AddString(DC_NAN);
    return true;
  }
  return false;
}

inline std::string DoubleConvert(const double& value, char *buffer,
                                 double_conversion::StringBuilder& builder, const double_conversion::DoubleToStringConverter& dc) {
  builder.Reset();
  if (double_conversion::Double(value).IsSpecial()) {
    HandleSpecialValues(value, builder);
    builder.Finalize();
    return buffer;
  }
  dc.ToPrecision(value, DC_PRECISION_REQUESTED, &builder);
  int pos = builder.position(); // get position before Finalize destroys it
  builder.Finalize();
  trimTrailingZeroes(buffer, pos);
  return buffer;
}

static uint32_t convert_to_uint32(const double d)
{
  auto ret = std::numeric_limits<uint32_t>::max();
  if (std::isfinite(d)) {
    try {
      ret = boost::numeric_cast<uint32_t>(d);
    } catch (boost::bad_numeric_cast&) {
      // ignore, leaving the default max() value
    }
  }
  return ret;
}

std::ostream& operator<<(std::ostream& stream, const Filename& filename)
{
  fs::path fnpath{static_cast<std::string>(filename)}; // gcc-4.6
  auto fpath = fs_uncomplete(fnpath, fs::current_path());
  stream << QuotedString(fpath.generic_string());
  return stream;
}

// FIXME: This could probably be done more elegantly using boost::regex
std::ostream& operator<<(std::ostream& stream, const QuotedString& s)
{
  stream << '"';
  for (char c : s) {
    switch (c) {
    case '\t': stream << "\\t"; break;
    case '\n': stream << "\\n"; break;
    case '\r': stream << "\\r"; break;
    case '"':  stream << "\\\""; break;
    case '\\': stream << "\\\\"; break;
    default:   stream << c;
    }
  }
  return stream << '"';
}

Value Value::clone() const {
  switch (this->type()) {
  case Type::UNDEFINED: return {};
  case Type::BOOL:      return std::get<bool>(this->value);
  case Type::NUMBER:    return std::get<double>(this->value);
  case Type::STRING:    return std::get<str_utf8_wrapper>(this->value).clone();
  case Type::RANGE:     return std::get<RangePtr>(this->value).clone();
  case Type::VECTOR:    return std::get<VectorType>(this->value).clone();
  case Type::OBJECT:    return std::get<ObjectType>(this->value).clone();
  case Type::FUNCTION:  return std::get<FunctionPtr>(this->value).clone();
  default: assert(false && "unknown Value variant type"); return {};
  }
}

Value Value::undef(const std::string& why)
{
  return Value{UndefType{why}};
}

std::string Value::typeName(Type type)
{
  switch (type) {
  case Type::UNDEFINED: return "undefined";
  case Type::BOOL:      return "bool";
  case Type::NUMBER:    return "number";
  case Type::STRING:    return "string";
  case Type::VECTOR:    return "vector";
  case Type::RANGE:     return "range";
  case Type::OBJECT:    return "object";
  case Type::FUNCTION:  return "function";
  default: assert(false && "unknown Value variant type"); return "<unknown>";
  }
}

const std::string Value::typeName() const
{
  return typeName(this->type());
}

// free functions for use by static_visitor templated functions in creating undef messages.
std::string getTypeName(const UndefType&) { return "undefined"; }
std::string getTypeName(bool) { return "bool"; }
std::string getTypeName(double) { return "number"; }
std::string getTypeName(const str_utf8_wrapper&) { return "string"; }
std::string getTypeName(const VectorType&) { return "vector"; }
std::string getTypeName(const ObjectType&) { return "object"; }
std::string getTypeName(const RangePtr&) { return "range"; }
std::string getTypeName(const FunctionPtr&) { return "function"; }

bool Value::toBool() const
{
  // NOLINTBEGIN(bugprone-branch-clone)
  switch (this->type()) {
  case Type::UNDEFINED: return false;
  case Type::BOOL:      return std::get<bool>(this->value);
  case Type::NUMBER:    return std::get<double>(this->value) != 0;
  case Type::STRING:    return !std::get<str_utf8_wrapper>(this->value).empty();
  case Type::VECTOR:    return !std::get<VectorType>(this->value).empty();
  case Type::RANGE:     return true;
  case Type::OBJECT:    return !std::get<ObjectType>(this->value).empty();
  case Type::FUNCTION:  return true;
  default: assert(false && "unknown Value variant type"); return false;
  }
  // NOLINTEND(bugprone-branch-clone)
}

// Convert the value to a double with an integer value, for use in bitwise operations.
// Since there are several possible ways to do this (floor, ceil, round, trunc) this function
// centralizes the choice for consistency.
double Value::toInteger() const
{
  return trunc(this->toDouble());
}

int64_t Value::toInt64() const
{
  return this->toInteger();
}

double Value::toDouble() const
{
  const double *d = std::get_if<double>(&this->value);
  return d ? *d : 0.0;
}

bool Value::getDouble(double& v) const
{
  const double *d = std::get_if<double>(&this->value);
  if (d) {
    v = *d;
    return true;
  }
  return false;
}

bool Value::getFiniteDouble(double& v) const
{
  double result;
  if (getDouble(result) && std::isfinite(result)) {
    v = result;
    return true;
  }
  return false;
}

bool Value::getUnsignedInt(unsigned int& v) const
{
  double result;
  if (getFiniteDouble(result) &&
      result >= 0.0 && result <= std::numeric_limits<unsigned int>::max()) {
    v = result;
    return true;
  }
  return false;
}

bool Value::getPositiveInt(unsigned int& v) const
{
  double result;
  if (getFiniteDouble(result) &&
      result >= 1 && result <= std::numeric_limits<unsigned int>::max()) {
    v = result;
    return true;
  }
  return false;
}

const str_utf8_wrapper& Value::toStrUtf8Wrapper() const {
  return std::get<str_utf8_wrapper>(this->value);
}

// Optimization to avoid multiple stream instantiations and copies to str for long vectors.
// Functions identically to "class tostring_visitor", except outputting to stream and not returning strings
class tostream_visitor
{
public:
  std::ostringstream& stream;
  mutable char buffer[DC_BUFFER_SIZE];
  mutable double_conversion::StringBuilder builder;
  double_conversion::DoubleToStringConverter dc;

  tostream_visitor(std::ostringstream& stream)
    : stream(stream), builder(buffer, DC_BUFFER_SIZE),
    dc(DC_FLAGS, DC_INF, DC_NAN, DC_EXP, DC_DECIMAL_LOW_EXP, DC_DECIMAL_HIGH_EXP, DC_MAX_LEADING_ZEROES, DC_MAX_TRAILING_ZEROES)
  {}

  template <typename T> void operator()(const T& op1) const {
    //std::cout << "[generic tostream_visitor]\n";
    stream << boost::lexical_cast<std::string>(op1);
  }

  void operator()(const double& op1) const {
    stream << DoubleConvert(op1, buffer, builder, dc);
  }

  void operator()(const UndefType&) const {
    stream << "undef";
  }

  void operator()(const bool& v) const {
    stream << (v ? "true" : "false");
  }

  void operator()(const EmbeddedVectorType&) const {
    assert(false && "Error: unexpected visit to EmbeddedVectorType!");
  }

  void operator()(const VectorType& v) const {
    if (StackCheck::inst().check()) {
      throw VectorEchoStringException::create();
    }
    stream << '[';
    if (!v.empty()) {
      auto it = v.begin();
      std::visit(*this, it->getVariant());
      for (++it; it != v.end(); ++it) {
        stream << ", ";
        std::visit(*this, it->getVariant());
      }
    }
    stream << ']';
  }

  void operator()(const ObjectType& v) const {
    if (StackCheck::inst().check()) {
      throw VectorEchoStringException::create();
    }
    stream << "{ ";
    for ( auto & key : v.keys()) {
      stream << key << " = ";
      std::visit(*this, v.get(key).getVariant());
      stream << "; ";
    }
    stream << '}';
  }
  
  void operator()(const str_utf8_wrapper& v) const {
    stream << '"' << v.toString() << '"';
  }

  void operator()(const RangePtr& v) const {
    stream << *v;
  }

  void operator()(const FunctionPtr& v) const {
    stream << *v;
  }
};

class tostring_visitor
{
public:
  template <typename T> std::string operator()(const T& op1) const {
    assert(false && "unhandled tostring_visitor type");
    return STR(op1);
  }

  std::string operator()(const str_utf8_wrapper& op1) const {
    return op1.toString();
  }

  std::string operator()(const double& op1) const {
    char buffer[DC_BUFFER_SIZE];
    double_conversion::StringBuilder builder(buffer, DC_BUFFER_SIZE);
    double_conversion::DoubleToStringConverter dc(DC_FLAGS, DC_INF, DC_NAN, DC_EXP,
                                                  DC_DECIMAL_LOW_EXP, DC_DECIMAL_HIGH_EXP, DC_MAX_LEADING_ZEROES, DC_MAX_TRAILING_ZEROES);
    return DoubleConvert(op1, buffer, builder, dc);
  }

  std::string operator()(const UndefType&) const {
    return "undef";
  }

  std::string operator()(const bool& v) const {
    return v ? "true" : "false";
  }

  std::string operator()(const EmbeddedVectorType&) const {
    assert(false && "Error: unexpected visit to EmbeddedVectorType!");
    return "";
  }

  std::string operator()(const VectorType& v) const {
    // Create a single stream and pass reference to it for list elements for optimization.
    std::ostringstream stream;
    try {
      (tostream_visitor(stream))(v);
    } catch (EvaluationException& e) {
      LOG(message_group::Error, e.what());
      throw;
    }
    return stream.str();
  }

  std::string operator()(const ObjectType& v) const {
    std::ostringstream stream;
    try {
      (tostream_visitor(stream))(v);
    } catch (EvaluationException& e) {
      LOG(message_group::Error, e.what());
      throw;
    }
    return stream.str();
  }

  std::string operator()(const RangePtr& v) const {
    return STR(*v);
  }

  std::string operator()(const FunctionPtr& v) const {
    return STR(*v);
  }
};

std::string Value::toString() const
{
  return std::visit(tostring_visitor(), this->value);
}

std::string Value::toEchoString() const
{
  if (type() == Value::Type::STRING) {
    return std::string("\"") + toString() + '"';
  } else {
    return toString();
  }
}

std::string Value::toEchoStringNoThrow() const
{
  std::string ret;
  try{
    ret = toEchoString();
  } catch (EvaluationException& e) {
    ret = "...";
  }
  return ret;
}

const UndefType& Value::toUndef() const
{
  return std::get<UndefType>(this->value);
}

std::string Value::toUndefString() const
{
  return std::get<UndefType>(this->value).toString();
}

class chr_visitor
{
public:
  template <typename S> std::string operator()(const S&) const
  {
    return "";
  }

  std::string operator()(const double& v) const
  {
    char buf[8];
    memset(buf, 0, 8);
    if (v > 0) {
      const gunichar c = v;
      if (g_unichar_validate(c) && (c != 0)) {
        g_unichar_to_utf8(c, buf);
      }
    }
    return {buf};
  }

  std::string operator()(const VectorType& v) const
  {
    std::ostringstream stream;
    for (auto& val : v) {
      stream << val.chrString();
    }
    return stream.str();
  }


  std::string operator()(const RangePtr& v) const
  {
    const uint32_t steps = v->numValues();
    if (steps >= RangeType::MAX_RANGE_STEPS) {
      LOG(message_group::Warning, "Bad range parameter in for statement: too many elements (%1$lu).", steps);
      return "";
    }

    std::ostringstream stream;
    for (double d : *v) stream << Value(d).chrString();
    return stream.str();
  }
};

std::string Value::chrString() const
{
  return std::visit(chr_visitor(), this->value);
}

VectorType::VectorType(EvaluationSession *session) :
  ptr(std::shared_ptr<VectorObject>(new VectorObject(), VectorObjectDeleter() ))
{
  ptr->evaluation_session = session;
}

VectorType::VectorType(class EvaluationSession *session, double x, double y, double z) :
  ptr(std::shared_ptr<VectorObject>(new VectorObject(), VectorObjectDeleter() ))
{
  ptr->evaluation_session = session;
  emplace_back(x);
  emplace_back(y);
  emplace_back(z);
}

void VectorType::emplace_back(Value&& val)
{
  if (val.type() == Value::Type::EMBEDDED_VECTOR) {
    emplace_back(std::move(val.toEmbeddedVectorNonConst()));
  } else {
    ptr->vec.push_back(std::move(val));
    if (ptr->evaluation_session) {
      ptr->evaluation_session->accounting().addVectorElement(1);
    }
  }
}

// Specialized handler for EmbeddedVectorTypes
void VectorType::emplace_back(EmbeddedVectorType&& mbed)
{
  if (mbed.size() > 1) {
    // embed_excess represents how many to add to vec.size() to get the total elements after flattening,
    // the embedded vector itself already counts towards an element in the parent's size, so subtract 1 from its size.
    ptr->embed_excess += mbed.size() - 1;
    ptr->vec.emplace_back(std::move(mbed));
    if (ptr->evaluation_session) {
      ptr->evaluation_session->accounting().addVectorElement(1);
    }
  } else if (mbed.size() == 1) {
    // If embedded vector contains only one value, then insert a copy of that element
    // Due to the above mentioned "-1" count, putting it in directaly as an EmbeddedVector
    // would not change embed_excess, which is needed to check if flatten is required.
    emplace_back(mbed.ptr->vec[0].clone());
  }
  // else mbed.size() == 0, do nothing
}

void VectorType::flatten() const
{
  vec_t ret;
  ret.reserve(this->size());
  // VectorType::iterator already handles the tricky recursive navigation of embedded vectors,
  // so just build up our new vector from that.
  for (const auto& el : *this) ret.emplace_back(el.clone());
  assert(ret.size() == this->size());
  ptr->embed_excess = 0;
  if (ptr->evaluation_session) {
    ptr->evaluation_session->accounting().addVectorElement(ret.size());
    ptr->evaluation_session->accounting().removeVectorElement(ptr->vec.size());
  }
  ptr->vec = std::move(ret);
}

void VectorType::VectorObjectDeleter::operator()(VectorObject *v)
{
  if (v->evaluation_session) {
    v->evaluation_session->accounting().removeVectorElement(v->vec.size());
  }

  VectorObject *orig = v;
  std::shared_ptr<VectorObject> curr;
  std::vector<std::shared_ptr<VectorObject>> purge;
  while (true) {
    if (v && v->embed_excess) {
      for (Value& val : v->vec) {
        auto type = val.type();
        if (type == Value::Type::EMBEDDED_VECTOR) {
          std::shared_ptr<VectorObject>& temp = std::get<EmbeddedVectorType>(val.value).ptr;
          if (temp.use_count() <= 1) purge.emplace_back(std::move(temp));
        } else if (type == Value::Type::VECTOR) {
          std::shared_ptr<VectorObject>& temp = std::get<VectorType>(val.value).ptr;
          if (temp.use_count() <= 1) purge.emplace_back(std::move(temp));
        }
      }
    }
    if (purge.empty()) break;
    curr = std::move(purge.back()); // this should cause destruction of the *previous value* for curr
    v = curr.get();
    purge.pop_back();
  }
  delete orig;
}

const VectorType& Value::toVector() const
{
  static const VectorType empty(nullptr);
  const VectorType *v = std::get_if<VectorType>(&this->value);
  return v ? *v : empty;
}

VectorType& Value::toVectorNonConst()
{
  return std::get<VectorType>(this->value);
}

const ObjectType& Value::toObject() const
{
  static const ObjectType empty(nullptr);
  const ObjectType *v = std::get_if<ObjectType>(&this->value);
  return v ? *v : empty;
}

EmbeddedVectorType& Value::toEmbeddedVectorNonConst()
{
  return std::get<EmbeddedVectorType>(this->value);
}

const EmbeddedVectorType& Value::toEmbeddedVector() const
{
  return std::get<EmbeddedVectorType>(this->value);
}

bool Value::getVec2(double& x, double& y, bool ignoreInfinite) const
{
  if (this->type() != Type::VECTOR) return false;
  const auto& v = this->toVector();
  if (v.size() != 2) return false;
  double rx, ry;
  bool valid = ignoreInfinite
    ? v[0].getFiniteDouble(rx) && v[1].getFiniteDouble(ry)
    : v[0].getDouble(rx) && v[1].getDouble(ry);
  if (valid) {
    x = rx;
    y = ry;
  }
  return valid;
}

bool Value::getVec3(double& x, double& y, double& z) const
{
  if (this->type() != Type::VECTOR) return false;
  const VectorType& v = this->toVector();
  if (v.size() != 3) return false;
  return (v[0].getDouble(x) && v[1].getDouble(y) && v[2].getDouble(z));
}

bool Value::getVec3(double& x, double& y, double& z, double defaultval) const
{
  if (this->type() != Type::VECTOR) return false;
  const VectorType& v = toVector();
  if (v.size() == 2) {
    getVec2(x, y);
    z = defaultval;
    return true;
  } else {
    if (v.size() != 3) return false;
  }
  return (v[0].getDouble(x) && v[1].getDouble(y) && v[2].getDouble(z));
}

const RangeType& Value::toRange() const
{
  const RangePtr *val = std::get_if<RangePtr>(&this->value);
  if (val) {
    return **val;
  } else return RangeType::EMPTY;
}

const FunctionType& Value::toFunction() const
{
  return *std::get<FunctionPtr>(this->value);
}

bool Value::isUncheckedUndef() const
{
  return this->type() == Type::UNDEFINED && !std::get<UndefType>(this->value).empty();
}

Value ObjectType::operator==(const ObjectType& other) const {
    if (other.ptr == this->ptr) {
        return true;
    }
    if (other.ptr->values.size() != this->ptr->values.size()) {
        return false;
    }

    for ( size_t i = 0; i< this->ptr->values.size(); i++){
        auto key_the_same = this->ptr->keys[i] != other.ptr->keys[i];
        if ( key_the_same ) {
            return false;
        }

        auto value_the_same = this->ptr->values[i] != other.ptr->values[i];
        if ( value_the_same.toBool() ) {
            return false;
        }
    }
    return true;
}
Value ObjectType::operator!=(const ObjectType& other) const {
    Value a = * this == other;
    return !a.toBool();
}

Value ObjectType::operator<(const ObjectType& /*other*/) const {
  return Value::undef("operation undefined (object < object)");
}
Value ObjectType::operator>(const ObjectType& /*other*/) const {
  return Value::undef("operation undefined (object > object)");
}
Value ObjectType::operator<=(const ObjectType& /*other*/) const {
  return Value::undef("operation undefined (object <= object)");
}
Value ObjectType::operator>=(const ObjectType& /*other*/) const {
  return Value::undef("operation undefined (object >= object)");
}

Value VectorType::operator==(const VectorType& v) const {
  size_t i = 0;
  auto first1 = this->begin(), last1 = this->end(), first2 = v.begin(), last2 = v.end();
  for ( ; (first1 != last1) && (first2 != last2); ++first1, ++first2, ++i) {
    Value temp = *first1 == *first2;
    if (temp.isUndefined()) {
      temp.toUndef().append(STR("in vector comparison at index ", i));
      return temp;
    }
    if (!temp.toBool()) return false;
  }
  return (first1 == last1) && (first2 == last2);
}

Value VectorType::operator!=(const VectorType& v) const {
  Value temp = this->VectorType::operator==(v);
  if (temp.isUndefined()) return temp;
  return !temp.toBool();
}

// lexicographical compare with possible undef result
Value VectorType::operator<(const VectorType& v) const {
  auto first1 = this->begin(), last1 = this->end(), first2 = v.begin(), last2 = v.end();
  size_t i = 0;
  for ( ; (first1 != last1) && (first2 != last2); ++first1, ++first2, ++i) {
    Value temp = *first1 < *first2;
    if (temp.isUndefined()) {
      temp.toUndef().append(STR("in vector comparison at index ", i));
      return temp;
    }
    if (temp.toBool()) return true;
    if ((*first2 < *first1).toBool()) return false;
  }
  return (first1 == last1) && (first2 != last2);
}

Value VectorType::operator>(const VectorType& v) const {
  return v.VectorType::operator<(*this);
}

Value VectorType::operator<=(const VectorType& v) const {
  Value temp = this->VectorType::operator>(v);
  if (temp.isUndefined()) return temp;
  return !temp.toBool();
}

Value VectorType::operator>=(const VectorType& v) const {
  Value temp = this->VectorType::operator<(v);
  if (temp.isUndefined()) return temp;
  return !temp.toBool();
}

class notequal_visitor
{
public:
  template <typename T, typename U> Value operator()(const T& /*op1*/, const U& /*op2*/) const { return true; }
  template <typename T> Value operator()(const T& op1, const T& op2) const { return op1 != op2; }
  Value operator()(const UndefType&, const UndefType&) const { return false; }
  template <typename T> Value operator()(const ValuePtr<T>& op1, const ValuePtr<T>& op2) const { return *op1 != *op2; }
};

class equals_visitor
{
public:
  template <typename T, typename U> Value operator()(const T& /*op1*/, const U& /*op2*/) const { return false; }
  template <typename T> Value operator()(const T& op1, const T& op2) const { return op1 == op2; }
  Value operator()(const UndefType&, const UndefType&) const { return true; }
  template <typename T> Value operator()(const ValuePtr<T>& op1, const ValuePtr<T>& op2) const { return *op1 == *op2; }
};

class less_visitor
{
public:
  template <typename T, typename U> Value operator()(const T& op1, const U& op2) const {
    return Value::undef(STR("undefined operation (", getTypeName(op1), " < ", getTypeName(op2), ")"));
  }
  template <typename T> Value operator()(const T& op1, const T& op2) const { return op1 < op2; }
  template <typename T> Value operator()(const ValuePtr<T>& op1, const ValuePtr<T>& op2) const { return *op1 < *op2; }
};

class greater_visitor
{
public:
  template <typename T, typename U> Value operator()(const T& op1, const U& op2) const {
    return Value::undef(STR("undefined operation (", getTypeName(op1), " > ", getTypeName(op2), ")"));
  }
  template <typename T> Value operator()(const T& op1, const T& op2) const { return op1 > op2; }
  template <typename T> Value operator()(const ValuePtr<T>& op1, const ValuePtr<T>& op2) const { return *op1 > *op2; }
};

class lessequal_visitor
{
public:
  template <typename T, typename U> Value operator()(const T& op1, const U& op2) const {
    return Value::undef(STR("undefined operation (", getTypeName(op1), " <= ", getTypeName(op2), ")"));
  }
  template <typename T> Value operator()(const T& op1, const T& op2) const { return op1 <= op2; }
  template <typename T> Value operator()(const ValuePtr<T>& op1, const ValuePtr<T>& op2) const { return *op1 <= *op2; }
};

class greaterequal_visitor
{
public:
  template <typename T, typename U> Value operator()(const T& op1, const U& op2) const {
    return Value::undef(STR("undefined operation (", getTypeName(op1), " >= ", getTypeName(op2), ")"));
  }
  template <typename T> Value operator()(const T& op1, const T& op2) const { return op1 >= op2; }
  template <typename T> Value operator()(const ValuePtr<T>& op1, const ValuePtr<T>& op2) const { return *op1 >= *op2; }
};

Value Value::operator==(const Value& v) const
{
  return std::visit(equals_visitor(), this->value, v.value);
}

Value Value::operator!=(const Value& v) const
{
  return std::visit(notequal_visitor(), this->value, v.value);
}

Value Value::operator<(const Value& v) const
{
  return std::visit(less_visitor(), this->value, v.value);
}

Value Value::operator>=(const Value& v) const
{
  return std::visit(greaterequal_visitor(), this->value, v.value);
}

Value Value::operator>(const Value& v) const
{
  return std::visit(greater_visitor(), this->value, v.value);
}

Value Value::operator<=(const Value& v) const
{
  return std::visit(lessequal_visitor(), this->value, v.value);
}

bool Value::cmp_less(const Value& v1, const Value& v2) {
  return v1.operator<(v2).toBool();
}

class plus_visitor
{
public:
  template <typename T, typename U> Value operator()(const T& op1, const U& op2) const {
    return Value::undef(STR("undefined operation (", getTypeName(op1), " + ", getTypeName(op2), ")"));
  }

  Value operator()(const double& op1, const double& op2) const {
    return op1 + op2;
  }

  Value operator()(const VectorType& op1, const VectorType& op2) const {
    VectorType sum(op1.evaluation_session());
    sum.reserve(op1.size());
    // FIXME: should we really truncate to shortest vector here?
    //   Maybe better to either "add zeroes" and return longest
    //   and/or issue an warning/error about length mismatch.
    for (auto it1 = op1.begin(), end1 = op1.end(), it2 = op2.begin(), end2 = op2.end();
         it1 != end1 && it2 != end2;
         ++it1, ++it2) {
      sum.emplace_back(*it1 + *it2);
    }
    return std::move(sum);
  }
};

Value Value::operator+(const Value& v) const
{
  return std::visit(plus_visitor(), this->value, v.value);
}

class minus_visitor
{
public:
  template <typename T, typename U> Value operator()(const T& op1, const U& op2) const {
    return Value::undef(STR("undefined operation (", getTypeName(op1), " - ", getTypeName(op2), ")"));
  }

  Value operator()(const double& op1, const double& op2) const {
    return op1 - op2;
  }

  Value operator()(const VectorType& op1, const VectorType& op2) const {
    VectorType sum(op1.evaluation_session());
    sum.reserve(op1.size());
    for (size_t i = 0; i < op1.size() && i < op2.size(); ++i) {
      sum.emplace_back(op1[i] - op2[i]);
    }
    return std::move(sum);
  }
};

Value Value::operator-(const Value& v) const
{
  return std::visit(minus_visitor(), this->value, v.value);
}

Value multvecnum(const VectorType& vecval, const Value& numval)
{
  // Vector * Number
  VectorType dstv(vecval.evaluation_session());
  dstv.reserve(vecval.size());
  for (const auto& val : vecval) {
    dstv.emplace_back(val * numval);
  }
  return std::move(dstv);
}

Value multmatvec(const VectorType& matrixvec, const VectorType& vectorvec)
{
  // Matrix * Vector
  VectorType dstv(matrixvec.evaluation_session());
  dstv.reserve(matrixvec.size());
  for (size_t i = 0; i < matrixvec.size(); ++i) {
    if (matrixvec[i].type() != Value::Type::VECTOR ||
        matrixvec[i].toVector().size() != vectorvec.size()) {
      return Value::undef(STR("Matrix must be rectangular. Problem at row ", i));
    }
    double r_e = 0.0;
    for (size_t j = 0; j < matrixvec[i].toVector().size(); ++j) {
      if (matrixvec[i].toVector()[j].type() != Value::Type::NUMBER) {
        return Value::undef(STR("Matrix must contain only numbers. Problem at row ", i, ", col ", j));
      }
      if (vectorvec[j].type() != Value::Type::NUMBER) {
        return Value::undef(STR("Vector must contain only numbers. Problem at index ", j));
      }
      r_e += matrixvec[i].toVector()[j].toDouble() * vectorvec[j].toDouble();
    }
    dstv.emplace_back(Value(r_e));
  }
  return std::move(dstv);
}

Value multvecmat(const VectorType& vectorvec, const VectorType& matrixvec)
{
  assert(vectorvec.size() == matrixvec.size());
  // Vector * Matrix
  VectorType dstv(matrixvec[0].toVector().evaluation_session());
  size_t firstRowSize = matrixvec[0].toVector().size();
  dstv.reserve(firstRowSize);
  for (size_t i = 0; i < firstRowSize; ++i) {
    double r_e = 0.0;
    for (size_t j = 0; j < vectorvec.size(); ++j) {
      if (matrixvec[j].type() != Value::Type::VECTOR ||
          matrixvec[j].toVector().size() != firstRowSize) {
        LOG(message_group::Warning, "Matrix must be rectangular. Problem at row %1$lu", j);
        return Value::undef(STR("Matrix must be rectangular. Problem at row ", j));
      }
      if (vectorvec[j].type() != Value::Type::NUMBER) {
        LOG(message_group::Warning, "Vector must contain only numbers. Problem at index %1$lu", j);
        return Value::undef(STR("Vector must contain only numbers. Problem at index ", j));
      }
      if (matrixvec[j].toVector()[i].type() != Value::Type::NUMBER) {
        LOG(message_group::Warning, "Matrix must contain only numbers. Problem at row %1$lu, col %2$lu", j, i);
        return Value::undef(STR("Matrix must contain only numbers. Problem at row ", j, ", col ", i));
      }
      r_e += vectorvec[j].toDouble() * matrixvec[j].toVector()[i].toDouble();
    }
    dstv.emplace_back(r_e);
  }
  return {std::move(dstv)};
}

Value multvecvec(const VectorType& vec1, const VectorType& vec2) {
  // Vector dot product.
  auto r = 0.0;
  for (size_t i = 0; i < vec1.size(); i++) {
    if (vec1[i].type() != Value::Type::NUMBER || vec2[i].type() != Value::Type::NUMBER) {
      return Value::undef(STR("undefined operation (", vec1[i].typeName(), " * ", vec2[i].typeName(), ")"));
    }
    r += vec1[i].toDouble() * vec2[i].toDouble();
  }
  return {r};
}

class multiply_visitor
{
public:
  template <typename T, typename U> Value operator()(const T& op1, const U& op2) const {
    return Value::undef(STR("undefined operation (", getTypeName(op1), " * ", getTypeName(op2), ")"));
  }
  Value operator()(const double& op1, const double& op2) const { return op1 * op2; }
  Value operator()(const double& op1, const VectorType& op2) const { return multvecnum(op2, op1); }
  Value operator()(const VectorType& op1, const double& op2) const { return multvecnum(op1, op2); }

  Value operator()(const VectorType& op1, const VectorType& op2) const {
    if (op1.empty() || op2.empty()) return Value::undef("Multiplication is undefined on empty vectors");
    auto first1 = op1.begin(), first2 = op2.begin();
    auto eltype1 = (*first1).type(), eltype2 = (*first2).type();
    if (eltype1 == Value::Type::NUMBER) {
      if (eltype2 == Value::Type::NUMBER) {
        if (op1.size() == op2.size()) return multvecvec(op1, op2);
        else return Value::undef(STR("vector*vector requires matching lengths (", op1.size(), " != ", op2.size(), ')'));
      } else if (eltype2 == Value::Type::VECTOR) {
        if (op1.size() == op2.size()) return multvecmat(op1, op2);
        else return Value::undef(STR("vector*matrix requires vector length to match matrix row count (", op1.size(), " != ", op2.size(), ')'));
      }
    } else if (eltype1 == Value::Type::VECTOR) {
      if (eltype2 == Value::Type::NUMBER) {
        if ((*first1).toVector().size() == op2.size()) return multmatvec(op1, op2);
        else return Value::undef(STR("matrix*vector requires matrix column count to match vector length (", (*first1).toVector().size(), " != ", op2.size(), ')'));
      } else if (eltype2 == Value::Type::VECTOR) {
        if ((*first1).toVector().size() == op2.size()) {
          // Matrix * Matrix
          VectorType dstv(op1.evaluation_session());
          dstv.reserve(op1.size());
          size_t i = 0;
          for (const auto& srcrow : op1) {
            const auto& srcrowvec = srcrow.toVector();
            if (srcrowvec.size() != op2.size()) return Value::undef(STR("matrix*matrix left operand row length does not match right operand row count (", srcrowvec.size(), " != ", op2.size(), ") at row ", i));
            auto temp = multvecmat(srcrowvec, op2);
            if (temp.isUndefined()) {
              temp.toUndef().append(STR("while processing left operand at row ", i));
              return temp;
            } else {
              dstv.emplace_back(std::move(temp));
            }
            ++i;
          }
          return {std::move(dstv)};
        } else {
          return Value::undef(STR("matrix*matrix requires left operand column count to match right operand row count (", (*first1).toVector().size(), " != ", op2.size(), ')'));
        }
      }
    }
    return Value::undef(STR("undefined vector*vector multiplication where first elements are types ", (*first1).typeName(), " and ", (*first2).typeName() ));
  }
};

Value Value::operator*(const Value& v) const
{
  return std::visit(multiply_visitor(), this->value, v.value);
}

Value Value::operator/(const Value& v) const
{
  if (this->type() == Type::NUMBER && v.type() == Type::NUMBER) {
    return this->toDouble() / v.toDouble();
  } else if (this->type() == Type::VECTOR && v.type() == Type::NUMBER) {
    VectorType dstv(this->toVector().evaluation_session());
    dstv.reserve(this->toVector().size());
    for (const auto& vecval : this->toVector()) {
      dstv.emplace_back(vecval / v);
    }
    return std::move(dstv);
  } else if (this->type() == Type::NUMBER && v.type() == Type::VECTOR) {
    VectorType dstv(v.toVector().evaluation_session());
    dstv.reserve(v.toVector().size());
    for (const auto& vecval : v.toVector()) {
      dstv.emplace_back(*this / vecval);
    }
    return std::move(dstv);
  }
  return Value::undef(STR("undefined operation (", this->typeName(), " / ", v.typeName(), ")"));
}

Value Value::operator%(const Value& v) const
{
  if (this->type() == Type::NUMBER && v.type() == Type::NUMBER) {
    return fmod(std::get<double>(this->value), std::get<double>(v.value));
  }
  return Value::undef(STR("undefined operation (", this->typeName(), " % ", v.typeName(), ")"));
}

Value Value::operator<<(const Value& v) const
{
  if (this->type() == Type::NUMBER && v.type() == Type::NUMBER) {
    int64_t lhs = this->toInt64();
    int64_t rhs = v.toInt64();
    if (rhs < 0) {
      return Value::undef(STR("negative shift"));
    }
    if (rhs >= 64) {
      return Value::undef(STR("shift too large"));
    }
    return (double)(lhs << rhs);
  }
  return Value::undef(STR("undefined operation (", this->typeName(), " << ", v.typeName(), ")"));
}

Value Value::operator>>(const Value& v) const
{
  if (this->type() == Type::NUMBER && v.type() == Type::NUMBER) {
    int64_t lhs = this->toInt64();
    int64_t rhs = v.toInt64();
    if (rhs < 0) {
      return Value::undef(STR("negative shift"));
    }
    if (rhs >= 64) {
      return Value::undef(STR("shift too large"));
    }
    return (double)(lhs >> rhs);
  }
  return Value::undef(STR("undefined operation (", this->typeName(), " >> ", v.typeName(), ")"));
}

Value Value::operator&(const Value& v) const
{
  if (this->type() == Type::NUMBER && v.type() == Type::NUMBER) {
    return (double)(this->toInt64() & v.toInt64());
  }
  return Value::undef(STR("undefined operation (", this->typeName(), " & ", v.typeName(), ")"));
}

Value Value::operator|(const Value& v) const
{
  if (this->type() == Type::NUMBER && v.type() == Type::NUMBER) {
    return (double)(this->toInt64() | v.toInt64());
  }
  return Value::undef(STR("undefined operation (", this->typeName(), " | ", v.typeName(), ")"));
}


Value Value::operator-() const
{
  if (this->type() == Type::NUMBER) {
    return {-this->toDouble()};
  } else if (this->type() == Type::VECTOR) {
    VectorType dstv(this->toVector().evaluation_session());
    dstv.reserve(this->toVector().size());
    for (const auto& vecval : this->toVector()) {
      dstv.emplace_back(-vecval);
    }
    return std::move(dstv);
  }
  return Value::undef(STR("undefined operation (-", this->typeName(), ")"));
}

Value Value::operator~() const
{
  if (this->type() == Type::NUMBER) {
    return (double)~this->toInt64();
  }
  return Value::undef(STR("undefined operation (~", this->typeName(), ")"));
}

Value Value::operator^(const Value& v) const
{
  if (this->type() == Type::NUMBER && v.type() == Type::NUMBER) {
    return {pow(std::get<double>(this->value), std::get<double>(v.value))};
  }
  return Value::undef(STR("undefined operation (", this->typeName(), " ^ ", v.typeName(), ")"));
}

/*
 * bracket operation [] detecting multi-byte unicode.
 * If the string is multi-byte unicode then the index will offset to the character (2 or 4 byte) and not to the byte.
 * A 'normal' string with byte chars are a subset of unicode and still work.
 */
class bracket_visitor
{
public:
  Value operator()(const str_utf8_wrapper& str, const double& idx) const {
    const auto i = convert_to_uint32(idx);
    auto unichar = str[i];
    if (unichar.empty()) {
      return Value::undefined.clone();
    }
    return unichar;
  }

  Value operator()(const VectorType& vec, const double& idx) const {
    const auto i = convert_to_uint32(idx);
    if (i < vec.size()) return vec[i].clone();
    return Value::undef(STR("index ", i, " out of bounds for vector of size ", vec.size()));
  }

  Value operator()(const ObjectType& obj, const str_utf8_wrapper& key) const {
    return obj[key].clone();
  }

  Value operator()(const RangePtr& range, const double& idx) const {
    const auto i = convert_to_uint32(idx);
    switch (i) {
    case 0: return range->begin_value();
    case 1: return range->step_value();
    case 2: return range->end_value();
    }
    return Value::undef("subscript operator only defined for indices 0-2 on range (begin,step,end)");
  }

  template <typename T, typename U> Value operator()(const T& op1, const U& op2) const {
    //std::cout << "generic bracket_visitor " << getTypeName(op1) << " " << getTypeName(op2) << "\n";
    return Value::undef(STR("undefined operation ", getTypeName(op1), "[", getTypeName(op2), "]"));
  }
};

Value Value::operator[](const Value& v) const
{
  return std::visit(bracket_visitor(), this->value, v.value);
}

Value Value::operator[](size_t idx) const
{
  Value v{(double)idx};
  return std::visit(bracket_visitor(), this->value, v.value);
}

std::ostream& operator<<(std::ostream& stream, const RangeType& r)
{
  char buffer[DC_BUFFER_SIZE];
  double_conversion::StringBuilder builder(buffer, DC_BUFFER_SIZE);
  double_conversion::DoubleToStringConverter dc(DC_FLAGS, DC_INF,
                                                DC_NAN, DC_EXP, DC_DECIMAL_LOW_EXP, DC_DECIMAL_HIGH_EXP,
                                                DC_MAX_LEADING_ZEROES, DC_MAX_TRAILING_ZEROES);
  return stream << "["
                << DoubleConvert(r.begin_value(), buffer, builder, dc) << " : "
                << DoubleConvert(r.step_value(), buffer, builder, dc) << " : "
                << DoubleConvert(r.end_value(),   buffer, builder, dc) << "]";
}

// called by clone()
ObjectType::ObjectType(const std::shared_ptr<ObjectObject>& copy)
  : ptr(copy)
{
}

ObjectType::ObjectType(EvaluationSession *session) :
  ptr(std::make_shared<ObjectObject>())
{
  ptr->evaluation_session = session;
}

const Value& ObjectType::get(const std::string& key) const              { return ptr->get(key); }
bool ObjectType::set(const std::string& key, Value value)               { return ptr->set(key,std::move(value)); }
bool ObjectType::del(const std::string& key)                            { return ptr->del(key) != NOINDEX; }
bool ObjectType::contains(const std::string& key) const                 { return ptr->find(key)!= NOINDEX; }
bool ObjectType::empty() const                                          { return ptr->values.empty(); }
const std::vector<std::string>& ObjectType::keys() const                { return ptr->keys; }
const std::vector<Value>& ObjectType::values() const                    { return ptr->values; }

const Value& ObjectType::operator[](const str_utf8_wrapper& v) const
{
  return this->get(v.toString());
}


// Copy explicitly only when necessary
ObjectType ObjectType::clone() const
{
  return ObjectType(this->ptr);
}

std::ostream& operator<<(std::ostream& stream, const ObjectType& v)
{
  stream << "{ ";
  auto iter = v.ptr->keys.begin();
  if (iter != v.ptr->keys.end()) {
    str_utf8_wrapper k(*iter);
    for (; iter != v.ptr->keys.end(); ++iter) {
      str_utf8_wrapper k2(*iter);
      stream << k2.toString() << " = " << v[k2] << "; ";
    }
  }
  stream << "}";
  return stream;
}
