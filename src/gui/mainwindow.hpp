#pragma once

#include <QMainWindow>
#include <QJsonObject>
#include <QByteArray>
#include <QString>

class IpcClient;
class QTabWidget;
class QListWidget;
class QListWidgetItem;
class QComboBox;
class QDoubleSpinBox;
class QSpinBox;
class QCheckBox;
class QLineEdit;
class QTableWidget;
class QTextEdit;
class QLabel;
class QPushButton;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    // IPC client signals
    void onConnectionStatusChanged(bool connected, quint64 version);
    void onConfigReceived(const QByteArray& json, quint64 version);
    void onConfigApplied(bool success, const QString& message, quint64 version);

    // Actions
    void onApplyClicked();
    void onSaveClicked();
    void onReloadClicked();
    void onPullFromGameClicked();
    void onResetDefaultsClicked();

    // Weapon tab slots
    void onWeaponSelected(int index);
    void onWeaponSearchChanged(const QString& text);
    void onWeaponCategoryChanged(int index);
    void onWeaponSkinComboChanged(int index);
    void onWeaponWearPresetChanged(int index);
    void onSaveWeaponClicked();
    void onClearWeaponClicked();
    void onWeaponTableCellDoubleClicked(int row, int column);

    // Knife tab slots
    void onKnifeCtSkinComboChanged(int index);
    void onKnifeTSkinComboChanged(int index);
    void onCopyCtToTClicked();
    void onCopyTToCtClicked();

    // Raw JSON tab slots
    void onApplyRawJsonClicked();
    void onFormatRawJsonClicked();
    void onResetAllWeaponsClicked();

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void setupUi();
    void setupTopBar();
    void setupWeaponsTab();
    void setupKnivesTab();
    void setupAgentsTab();
    void setupSettingsTab();
    void populateCatalogs();

    void loadConfigFromJson(const QByteArray& jsonBytes);
    QByteArray buildConfigJson() const;
    void refreshUiFromConfig();
    void refreshWeaponTable();
    void updateWeaponEditor(const QString& weaponId);
    void setDirty(bool dirty);

    IpcClient* ipcClient_ = nullptr;
    QJsonObject currentConfig_;
    bool isDirty_ = false;
    bool updatingUi_ = false;

    // Top bar
    QLabel* statusBadgeLabel_ = nullptr;
    QPushButton* applyBtn_ = nullptr;
    QPushButton* saveBtn_ = nullptr;
    QPushButton* reloadBtn_ = nullptr;
    QPushButton* pullBtn_ = nullptr;

    // Tabs
    QTabWidget* tabWidget_ = nullptr;

    // Weapons Tab Widgets
    QLineEdit* weaponSearchEdit_ = nullptr;
    QComboBox* weaponCategoryCombo_ = nullptr;
    QListWidget* weaponListWidget_ = nullptr;
    QLabel* weaponTitleLabel_ = nullptr;
    QCheckBox* weaponEnableCheck_ = nullptr;
    QComboBox* weaponSkinCombo_ = nullptr;
    QSpinBox* weaponPaintKitSpin_ = nullptr;
    QComboBox* weaponWearPresetCombo_ = nullptr;
    QDoubleSpinBox* weaponWearSpin_ = nullptr;
    QSpinBox* weaponSeedSpin_ = nullptr;
    QPushButton* weaponSaveBtn_ = nullptr;
    QPushButton* weaponClearBtn_ = nullptr;
    QTableWidget* weaponSummaryTable_ = nullptr;

    // Knives Tab Widgets
    QCheckBox* knifeCtEnableCheck_ = nullptr;
    QComboBox* knifeCtModelCombo_ = nullptr;
    QComboBox* knifeCtSkinCombo_ = nullptr;
    QSpinBox* knifeCtPaintKitSpin_ = nullptr;
    QDoubleSpinBox* knifeCtWearSpin_ = nullptr;
    QSpinBox* knifeCtSeedSpin_ = nullptr;

    QCheckBox* knifeTEnableCheck_ = nullptr;
    QComboBox* knifeTModelCombo_ = nullptr;
    QComboBox* knifeTSkinCombo_ = nullptr;
    QSpinBox* knifeTPaintKitSpin_ = nullptr;
    QDoubleSpinBox* knifeTWearSpin_ = nullptr;
    QSpinBox* knifeTSeedSpin_ = nullptr;

    // Agents Tab Widgets
    QCheckBox* agentEnableCheck_ = nullptr;
    QComboBox* agentCtCombo_ = nullptr;
    QComboBox* agentTCombo_ = nullptr;

    // Settings / JSON Tab Widgets
    QTextEdit* rawJsonEdit_ = nullptr;
    QLabel* socketPathLabel_ = nullptr;
    QLabel* configPathLabel_ = nullptr;

    QString currentSelectedWeaponId_;
};
