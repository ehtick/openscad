#pragma once

#include <QAction>
#include <QActionGroup>
#include <QCloseEvent>
#include <QComboBox>
#include <QFont>
#include <QFontComboBox>
#include <QHash>
#include <QKeyEvent>
#include <QShowEvent>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QWidget>
#include <QMainWindow>
#include <QSettings>
#include <string>

#include "gui/qtgettext.h" // IWYU pragma: keep
#include "openscad_gui.h"
#include "ui_Preferences.h"
#include "core/Settings.h"
#include "gui/InitConfigurator.h"

class GlobalPreferences;
class Preferences : public QMainWindow, public Ui::Preferences, public InitConfigurator
{
  Q_OBJECT;

public:
  ~Preferences() override;

  QVariant getValue(const QString& key) const;
  void init();
  void update();
  void apply_win() const;
  void updateGUI();
  void fireEditorConfigChanged() const;
  void insertListItem(QListWidget *listBox, QListWidgetItem *listItem);

  // Returns true if there is an higlightling color scheme configured.
  bool hasHighlightingColorScheme() const;

  // Set a new colorScheme.
  void setHighlightingColorSchemes(const QStringList& colorSchemes);

public slots:
  void actionTriggered(class QAction *);
  void featuresCheckBoxToggled(bool);
  void on_stackedWidget_currentChanged(int);
  void on_colorSchemeChooser_itemSelectionChanged();
  void on_fontChooser_currentFontChanged(const QFont&);
  void on_fontSize_currentIndexChanged(int);
  void on_syntaxHighlight_currentTextChanged(const QString&);
  void on_openCSGWarningBox_toggled(bool);
  void on_cgalCacheSizeMBEdit_textChanged(const QString&);
  void on_polysetCacheSizeMBEdit_textChanged(const QString&);
  void on_opencsgLimitEdit_textChanged(const QString&);
  void on_forceGoldfeatherBox_toggled(bool);
  void on_mouseWheelZoomBox_toggled(bool);
  void on_localizationCheckBox_toggled(bool);
  void on_autoReloadRaiseCheckBox_toggled(bool);
  void on_updateCheckBox_toggled(bool);
  void on_snapshotCheckBox_toggled(bool);
  void on_reorderCheckBox_toggled(bool);
  void on_undockCheckBox_toggled(bool);
  void on_checkNowButton_clicked();
  void on_launcherBox_toggled(bool);
  void on_enableSoundOnRenderCompleteCheckBox_toggled(bool);
  void on_enableHardwarningsCheckBox_toggled(bool);
  void on_traceDepthEdit_textChanged(const QString&);
  void on_enableTraceUsermoduleParametersCheckBox_toggled(bool);
  void on_enableParameterCheckBox_toggled(bool);
  void on_enableRangeCheckBox_toggled(bool);
  void on_comboBoxRenderBackend3D_activated(int);
  void on_comboBoxToolbarExport3D_activated(int);
  void on_comboBoxToolbarExport2D_activated(int);
  void on_checkBoxSummaryCamera_toggled(bool);
  void on_checkBoxSummaryArea_toggled(bool);
  void on_checkBoxSummaryBoundingBox_toggled(bool);
  void on_enableHidapiTraceCheckBox_toggled(bool);
  void on_checkBoxShowWarningsIn3dView_toggled(bool);
  void on_checkBoxMouseCentricZoom_toggled(bool);
  void on_checkBoxMouseSwapButtons_toggled(bool);
  void on_timeThresholdOnRenderCompleteSoundEdit_textChanged(const QString&);
  void on_enableClearConsoleCheckBox_toggled(bool);
  void on_consoleMaxLinesEdit_textChanged(const QString&);
  void on_consoleFontChooser_currentFontChanged(const QFont&);
  void on_consoleFontSize_currentIndexChanged(int);
  void on_customizerFontChooser_currentFontChanged(const QFont&);
  void on_customizerFontSize_currentIndexChanged(int);
  void on_checkBoxEnableAutocomplete_toggled(bool);
  void on_lineEditCharacterThreshold_textChanged(const QString&);
  //
  // editor settings
  //

  // Indentation
  void on_checkBoxAutoIndent_toggled(bool);
  void on_checkBoxBackspaceUnindents_toggled(bool);
  void on_comboBoxIndentUsing_activated(int);
  void on_spinBoxIndentationWidth_valueChanged(int);
  void on_spinBoxTabWidth_valueChanged(int);
  void on_comboBoxTabKeyFunction_activated(int);
  void on_comboBoxShowWhitespace_activated(int);
  void on_spinBoxShowWhitespaceSize_valueChanged(int);

  // Line wrap
  void on_comboBoxLineWrap_activated(int);
  void on_comboBoxLineWrapIndentationStyle_activated(int);
  void on_spinBoxLineWrapIndentationIndent_valueChanged(int);
  void on_comboBoxLineWrapVisualizationStart_activated(int);
  void on_comboBoxLineWrapVisualizationEnd_activated(int);
  void on_comboBoxModifierNumberScrollWheel_activated(int);


  // Display
  void on_checkBoxHighlightCurrentLine_toggled(bool);
  void on_checkBoxEnableBraceMatching_toggled(bool);
  void on_checkBoxEnableLineNumbers_toggled(bool);

  // Print
  void on_checkBoxEnableRemotePrintServices_toggled(bool);
  void on_comboBoxDefaultPrintService_activated(int);
  void on_pushButtonOctoPrintCheckConnection_clicked();
  void on_pushButtonOctoPrintSlicingEngine_clicked();
  void on_comboBoxOctoPrintSlicingEngine_activated(int);
  void on_pushButtonOctoPrintSlicingProfile_clicked();
  void on_comboBoxOctoPrintSlicingProfile_activated(int);
  void on_comboBoxOctoPrintAction_activated(int);
  void on_comboBoxOctoPrintFileFormat_activated(int);
  void on_lineEditOctoPrintURL_editingFinished();
  void on_lineEditOctoPrintApiKey_editingFinished();
  void on_pushButtonOctoPrintApiKey_clicked();
  void on_pushButtonOctoPrintRequestApiKey_clicked();
  void on_lineEditLocalAppExecutable_editingFinished();
  void on_toolButtonLocalAppSelectExecutable_clicked();
  void on_lineEditLocalAppTempDir_editingFinished();
  void on_toolButtonLocalAppSelectTempDir_clicked();
  void on_comboBoxLocalAppFileFormat_activated(int);
  void on_toolButtonLocalAppParameterRemove_clicked();
  void on_toolButtonLocalAppParameterAdd_clicked();
  void on_toolButtonLocalAppParameterUp_clicked();
  void on_toolButtonLocalAppParameterDown_clicked();
  void on_toolButtonLocalAppParameterAddFile_clicked();
  void on_listWidgetLocalAppParams_itemSelectionChanged();
  void on_listWidgetLocalAppParams_itemChanged(QListWidgetItem *);
  void on_actionLocalAppParameterFile_triggered();
  void on_actionLocalAppParameterDir_triggered();
  void on_actionLocalAppParameterExtension_triggered();
  void on_actionLocalAppParameterSource_triggered();
  void on_actionLocalAppParameterSourceDir_triggered();
  void listWidgetLocalAppParamsModelDataChanged();

  // Dialogs
  void on_checkBoxAlwaysShowExportPdfDialog_toggled(bool);
  void on_checkBoxAlwaysShowExport3mfDialog_toggled(bool);
  void on_checkBoxAlwaysShowPrintServiceDialog_toggled(bool);

signals:
  void requestRedraw() const;
  void updateUndockMode(bool undockMode) const;
  void updateReorderMode(bool undockMode) const;
  void fontChanged(const QString& family, uint size) const;
  void consoleFontChanged(const QString& family, uint size) const;
  void customizerFontChanged(const QString& family, uint size) const;
  void colorSchemeChanged(const QString& scheme) const;
  void openCSGSettingsChanged() const;
  void syntaxHighlightChanged(const QString& s) const;
  void editorConfigChanged() const;
  void ExperimentalChanged() const;
  void updateMouseCentricZoom(bool state) const;
  void updateMouseSwapButtons(bool state) const;
  void autocompleteChanged(bool status) const;
  void characterThresholdChanged(int val) const;
  void stepSizeChanged(int val) const;
  void toolbarExportChanged() const;

private slots:
  void on_lineEditStepSize_textChanged(const QString& arg1);

  void on_checkBoxEnableNumberScrollWheel_toggled(bool checked);

private:
  friend GlobalPreferences;
  Preferences(QWidget *parent = nullptr);
  void keyPressEvent(QKeyEvent *e) override;
  void showEvent(QShowEvent *e) override;
  void closeEvent(QCloseEvent *e) override;
  void removeDefaultSettings();
  void setupFeaturesPage();
  void setup3DPrintPage();
  void writeSettings();
  void hidePasswords();
  void addPrefPage(QActionGroup *group, QAction *action, QWidget *widget);
  void createFontSizeMenu(QComboBox *box, const QString &setting);
  void updateGUIFontFamily(QFontComboBox *fontSelector, const QString &setting);
  void updateGUIFontSize(QComboBox *fsSelector, const QString &setting);
  void updateLocalAppParams();
  void addLocalAppParameter(const Settings::LocalAppParameterType&);
  void moveListBoxRow(QListWidget *listBox, int offset);

  /** Set value from combobox to settings */
  void applyComboBox(QComboBox *comboBox, int val, Settings::SettingsEntryEnum<std::string>& entry);

  QSettings::SettingsMap defaultmap;
  QHash<const QAction *, QWidget *> prefPages;
};

class GlobalPreferences
{
public:
    static Preferences* inst();
};
