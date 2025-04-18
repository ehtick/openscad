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

#include "core/ColorNode.h"
#include "geometry/linalg.h"
#include "core/module.h"
#include "core/ModuleInstantiation.h"
#include "core/Builtins.h"
#include "core/Children.h"
#include "core/Parameters.h"
#include "core/ColorUtil.h"
#include "utils/printutils.h"
#include <algorithm>
#include <utility>
#include <memory>
#include <cctype>
#include <cstddef>
#include <string>
#include <iterator>
#include <unordered_map>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/assign/std/vector.hpp>
#include <boost/assign/list_of.hpp>
using namespace boost::assign; // bring 'operator+=()' into scope

// Colors extracted from https://drafts.csswg.org/css-color/ on 2015-08-02
// CSS Color Module Level 4 - Editor’s Draft, 29 May 2015
std::unordered_map<std::string, Color4f> webcolors{
  {"aliceblue", {240, 248, 255}},
  {"antiquewhite", {250, 235, 215}},
  {"aqua", {0, 255, 255}},
  {"aquamarine", {127, 255, 212}},
  {"azure", {240, 255, 255}},
  {"beige", {245, 245, 220}},
  {"bisque", {255, 228, 196}},
  {"black", {0, 0, 0}},
  {"blanchedalmond", {255, 235, 205}},
  {"blue", {0, 0, 255}},
  {"blueviolet", {138, 43, 226}},
  {"brown", {165, 42, 42}},
  {"burlywood", {222, 184, 135}},
  {"cadetblue", {95, 158, 160}},
  {"chartreuse", {127, 255, 0}},
  {"chocolate", {210, 105, 30}},
  {"coral", {255, 127, 80}},
  {"cornflowerblue", {100, 149, 237}},
  {"cornsilk", {255, 248, 220}},
  {"crimson", {220, 20, 60}},
  {"cyan", {0, 255, 255}},
  {"darkblue", {0, 0, 139}},
  {"darkcyan", {0, 139, 139}},
  {"darkgoldenrod", {184, 134, 11}},
  {"darkgray", {169, 169, 169}},
  {"darkgreen", {0, 100, 0}},
  {"darkgrey", {169, 169, 169}},
  {"darkkhaki", {189, 183, 107}},
  {"darkmagenta", {139, 0, 139}},
  {"darkolivegreen", {85, 107, 47}},
  {"darkorange", {255, 140, 0}},
  {"darkorchid", {153, 50, 204}},
  {"darkred", {139, 0, 0}},
  {"darksalmon", {233, 150, 122}},
  {"darkseagreen", {143, 188, 143}},
  {"darkslateblue", {72, 61, 139}},
  {"darkslategray", {47, 79, 79}},
  {"darkslategrey", {47, 79, 79}},
  {"darkturquoise", {0, 206, 209}},
  {"darkviolet", {148, 0, 211}},
  {"deeppink", {255, 20, 147}},
  {"deepskyblue", {0, 191, 255}},
  {"dimgray", {105, 105, 105}},
  {"dimgrey", {105, 105, 105}},
  {"dodgerblue", {30, 144, 255}},
  {"firebrick", {178, 34, 34}},
  {"floralwhite", {255, 250, 240}},
  {"forestgreen", {34, 139, 34}},
  {"fuchsia", {255, 0, 255}},
  {"gainsboro", {220, 220, 220}},
  {"ghostwhite", {248, 248, 255}},
  {"gold", {255, 215, 0}},
  {"goldenrod", {218, 165, 32}},
  {"gray", {128, 128, 128}},
  {"green", {0, 128, 0}},
  {"greenyellow", {173, 255, 47}},
  {"grey", {128, 128, 128}},
  {"honeydew", {240, 255, 240}},
  {"hotpink", {255, 105, 180}},
  {"indianred", {205, 92, 92}},
  {"indigo", {75, 0, 130}},
  {"ivory", {255, 255, 240}},
  {"khaki", {240, 230, 140}},
  {"lavender", {230, 230, 250}},
  {"lavenderblush", {255, 240, 245}},
  {"lawngreen", {124, 252, 0}},
  {"lemonchiffon", {255, 250, 205}},
  {"lightblue", {173, 216, 230}},
  {"lightcoral", {240, 128, 128}},
  {"lightcyan", {224, 255, 255}},
  {"lightgoldenrodyellow", {250, 250, 210}},
  {"lightgray", {211, 211, 211}},
  {"lightgreen", {144, 238, 144}},
  {"lightgrey", {211, 211, 211}},
  {"lightpink", {255, 182, 193}},
  {"lightsalmon", {255, 160, 122}},
  {"lightseagreen", {32, 178, 170}},
  {"lightskyblue", {135, 206, 250}},
  {"lightslategray", {119, 136, 153}},
  {"lightslategrey", {119, 136, 153}},
  {"lightsteelblue", {176, 196, 222}},
  {"lightyellow", {255, 255, 224}},
  {"lime", {0, 255, 0}},
  {"limegreen", {50, 205, 50}},
  {"linen", {250, 240, 230}},
  {"magenta", {255, 0, 255}},
  {"maroon", {128, 0, 0}},
  {"mediumaquamarine", {102, 205, 170}},
  {"mediumblue", {0, 0, 205}},
  {"mediumorchid", {186, 85, 211}},
  {"mediumpurple", {147, 112, 219}},
  {"mediumseagreen", {60, 179, 113}},
  {"mediumslateblue", {123, 104, 238}},
  {"mediumspringgreen", {0, 250, 154}},
  {"mediumturquoise", {72, 209, 204}},
  {"mediumvioletred", {199, 21, 133}},
  {"midnightblue", {25, 25, 112}},
  {"mintcream", {245, 255, 250}},
  {"mistyrose", {255, 228, 225}},
  {"moccasin", {255, 228, 181}},
  {"navajowhite", {255, 222, 173}},
  {"navy", {0, 0, 128}},
  {"oldlace", {253, 245, 230}},
  {"olive", {128, 128, 0}},
  {"olivedrab", {107, 142, 35}},
  {"orange", {255, 165, 0}},
  {"orangered", {255, 69, 0}},
  {"orchid", {218, 112, 214}},
  {"palegoldenrod", {238, 232, 170}},
  {"palegreen", {152, 251, 152}},
  {"paleturquoise", {175, 238, 238}},
  {"palevioletred", {219, 112, 147}},
  {"papayawhip", {255, 239, 213}},
  {"peachpuff", {255, 218, 185}},
  {"peru", {205, 133, 63}},
  {"pink", {255, 192, 203}},
  {"plum", {221, 160, 221}},
  {"powderblue", {176, 224, 230}},
  {"purple", {128, 0, 128}},
  {"rebeccapurple", {102, 51, 153}},
  {"red", {255, 0, 0}},
  {"rosybrown", {188, 143, 143}},
  {"royalblue", {65, 105, 225}},
  {"saddlebrown", {139, 69, 19}},
  {"salmon", {250, 128, 114}},
  {"sandybrown", {244, 164, 96}},
  {"seagreen", {46, 139, 87}},
  {"seashell", {255, 245, 238}},
  {"sienna", {160, 82, 45}},
  {"silver", {192, 192, 192}},
  {"skyblue", {135, 206, 235}},
  {"slateblue", {106, 90, 205}},
  {"slategray", {112, 128, 144}},
  {"slategrey", {112, 128, 144}},
  {"snow", {255, 250, 250}},
  {"springgreen", {0, 255, 127}},
  {"steelblue", {70, 130, 180}},
  {"tan", {210, 180, 140}},
  {"teal", {0, 128, 128}},
  {"thistle", {216, 191, 216}},
  {"tomato", {255, 99, 71}},
  {"turquoise", {64, 224, 208}},
  {"violet", {238, 130, 238}},
  {"wheat", {245, 222, 179}},
  {"white", {255, 255, 255}},
  {"whitesmoke", {245, 245, 245}},
  {"yellow", {255, 255, 0}},
  {"yellowgreen", {154, 205, 50}},

  // additional OpenSCAD specific entry
  {"transparent", {0, 0, 0, 0}}
};

static std::shared_ptr<AbstractNode> builtin_color(const ModuleInstantiation *inst, Arguments arguments, const Children& children)
{
  auto node = std::make_shared<ColorNode>(inst);

  Parameters parameters = Parameters::parse(std::move(arguments), inst->location(), {"c", "alpha"});
  if (parameters["c"].type() == Value::Type::VECTOR) {
    const auto& vec = parameters["c"].toVector();
    for (size_t i = 0; i < 4; ++i) {
      node->color[i] = i < vec.size() ? (float)vec[i].toDouble() : 1.0f;
      if (node->color[i] > 1 || node->color[i] < 0) {
        LOG(message_group::Warning, inst->location(), parameters.documentRoot(), "color() expects numbers between 0.0 and 1.0. Value of %1$.1f is out of range", node->color[i]);
      }
    }
  } else if (parameters["c"].type() == Value::Type::STRING) {
    auto colorname = parameters["c"].toString();
    boost::algorithm::to_lower(colorname);
    if (webcolors.find(colorname) != webcolors.end()) {
      node->color = webcolors.at(colorname);
    } else {
      // Try parsing it as a hex color such as "#rrggbb".
      const auto hexColor = OpenSCAD::parse_hex_color(colorname);
      if (hexColor) {
        node->color = *hexColor;
      } else {
        LOG(message_group::Warning, inst->location(), parameters.documentRoot(), "Unable to parse color \"%1$s\"", colorname);
        LOG("Please see https://en.wikipedia.org/wiki/Web_colors");
      }
    }
  }
  if (parameters["alpha"].type() == Value::Type::NUMBER) {
    node->color[3] = parameters["alpha"].toDouble();
  }

  return children.instantiate(node);
}

std::string ColorNode::toString() const
{
  return STR("color([", this->color[0], ", ", this->color[1], ", ", this->color[2], ", ", this->color[3], "])");
}

std::string ColorNode::name() const
{
  return "color";
}

void register_builtin_color()
{
  Builtins::init("color", new BuiltinModule(builtin_color),
  {
    "color(c = [r, g, b, a])",
    "color(c = [r, g, b], alpha = 1.0)",
    "color(\"#hexvalue\")",
    "color(\"colorname\", 1.0)",
  });
}
