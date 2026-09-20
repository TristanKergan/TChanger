#include "mainwindow.hpp"
#include "ipc_client.hpp"

#include <QApplication>
#include <QBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QListWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QTextEdit>
#include <QStatusBar>
#include <QMessageBox>
#include <QSplitter>
#include <QFrame>
#include <QShortcut>
#include <QKeySequence>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QFont>
#include <QColor>
#include <QTime>
#include <QCloseEvent>

import ItemCatalog;

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ipcClient_(new IpcClient(this)) {
    setupUi();
    populateCatalogs();

    connect(ipcClient_, &IpcClient::connectionStatusChanged, this, &MainWindow::onConnectionStatusChanged);
    connect(ipcClient_, &IpcClient::configReceived, this, &MainWindow::onConfigReceived);
    connect(ipcClient_, &IpcClient::configApplied, this, &MainWindow::onConfigApplied);

    // Initial load from disk
    QByteArray diskJson;
    if (IpcClient::loadFromDisk(diskJson)) {
        loadConfigFromJson(diskJson);
        statusBar()->showMessage(QStringLiteral("Config loaded from ") + IpcClient::defaultConfigFile(), 3000);
    } else {
        onResetDefaultsClicked();
        statusBar()->showMessage(QStringLiteral("Using default config"), 3000);
    }

    // Trigger initial connection check
    ipcClient_->checkConnection();
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUi() {
    setWindowTitle(QStringLiteral("Hamzex CS2 Configurator & Live Hot-Reload"));
    resize(980, 720);
    setMinimumSize(850, 600);

    auto* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    auto* rootLayout = new QVBoxLayout(centralWidget);
    rootLayout->setContentsMargins(12, 10, 12, 10);
    rootLayout->setSpacing(10);

    setupTopBar();
    rootLayout->addWidget(statusBadgeLabel_->parentWidget());

    tabWidget_ = new QTabWidget(this);
    setupWeaponsTab();
    setupKnivesTab();
    setupAgentsTab();
    setupSettingsTab();

    rootLayout->addWidget(tabWidget_, 1);

    // Shortcuts
    auto* applyShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Return), this);
    connect(applyShortcut, &QShortcut::activated, this, &MainWindow::onApplyClicked);

    auto* saveShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_S), this);
    connect(saveShortcut, &QShortcut::activated, this, &MainWindow::onSaveClicked);

    auto* reloadShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_R), this);
    connect(reloadShortcut, &QShortcut::activated, this, &MainWindow::onReloadClicked);

    auto* searchShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_F), this);
    connect(searchShortcut, &QShortcut::activated, this, [this]() {
        tabWidget_->setCurrentIndex(0);
        weaponSearchEdit_->setFocus();
        weaponSearchEdit_->selectAll();
    });

    for (int t = 0; t < 4; ++t) {
        auto* tabShortcut = new QShortcut(QKeySequence(Qt::CTRL | (Qt::Key_1 + t)), this);
        connect(tabShortcut, &QShortcut::activated, this, [this, t]() {
            tabWidget_->setCurrentIndex(t);
        });
    }
}

void MainWindow::setupTopBar() {
    auto* topContainer = new QFrame(this);
    topContainer->setFrameShape(QFrame::StyledPanel);
    topContainer->setStyleSheet(QStringLiteral(
        "QFrame { background-color: #202225; border: 1px solid #32353b; border-radius: 6px; padding: 6px; }"
    ));

    auto* topLayout = new QHBoxLayout(topContainer);
    topLayout->setContentsMargins(8, 4, 8, 4);
    topLayout->setSpacing(10);

    statusBadgeLabel_ = new QLabel(this);
    statusBadgeLabel_->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 13px; color: #f0ad4e;"));
    statusBadgeLabel_->setText(QStringLiteral("○ CS2 Runtime: Checking connection..."));
    topLayout->addWidget(statusBadgeLabel_, 1);

    pullBtn_ = new QPushButton(QStringLiteral("📥 Pull from Game"), this);
    pullBtn_->setToolTip(QStringLiteral("Fetch the running config currently active in CS2 memory"));
    pullBtn_->setEnabled(false);
    connect(pullBtn_, &QPushButton::clicked, this, &MainWindow::onPullFromGameClicked);
    topLayout->addWidget(pullBtn_);

    reloadBtn_ = new QPushButton(QStringLiteral("🔄 Reload"), this);
    reloadBtn_->setToolTip(QStringLiteral("Reload configuration from disk (Ctrl+R)"));
    connect(reloadBtn_, &QPushButton::clicked, this, &MainWindow::onReloadClicked);
    topLayout->addWidget(reloadBtn_);

    saveBtn_ = new QPushButton(QStringLiteral("💾 Save File"), this);
    saveBtn_->setToolTip(QStringLiteral("Save configuration to ~/.config/hamzex/config.json (Ctrl+S)"));
    connect(saveBtn_, &QPushButton::clicked, this, &MainWindow::onSaveClicked);
    topLayout->addWidget(saveBtn_);

    applyBtn_ = new QPushButton(QStringLiteral("⚡ Apply to Game"), this);
    applyBtn_->setToolTip(QStringLiteral("Apply changes live into CS2 via local IPC (Ctrl+Enter)"));
    applyBtn_->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #2d7d46; color: white; font-weight: bold; padding: 6px 14px; border-radius: 4px; }"
        "QPushButton:hover { background-color: #389b57; }"
        "QPushButton:pressed { background-color: #246538; }"
    ));
    connect(applyBtn_, &QPushButton::clicked, this, &MainWindow::onApplyClicked);
    topLayout->addWidget(applyBtn_);
}

void MainWindow::setupWeaponsTab() {
    auto* tab = new QWidget(this);
    auto* tabLayout = new QVBoxLayout(tab);
    tabLayout->setContentsMargins(8, 8, 8, 8);
    tabLayout->setSpacing(8);

    auto* splitter = new QSplitter(Qt::Horizontal, tab);

    // Left panel: weapon list + filter
    auto* leftWidget = new QWidget(splitter);
    auto* leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(0, 0, 4, 0);
    leftLayout->setSpacing(6);

    weaponSearchEdit_ = new QLineEdit(leftWidget);
    weaponSearchEdit_->setPlaceholderText(QStringLiteral("🔍 Filter weapons..."));
    connect(weaponSearchEdit_, &QLineEdit::textChanged, this, &MainWindow::onWeaponSearchChanged);
    leftLayout->addWidget(weaponSearchEdit_);

    weaponCategoryCombo_ = new QComboBox(leftWidget);
    weaponCategoryCombo_->addItems({
        QStringLiteral("All Weapons"),
        QStringLiteral("Rifles"),
        QStringLiteral("Pistols"),
        QStringLiteral("Snipers"),
        QStringLiteral("SMGs"),
        QStringLiteral("Heavy / Shotguns")
    });
    connect(weaponCategoryCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onWeaponCategoryChanged);
    leftLayout->addWidget(weaponCategoryCombo_);

    weaponListWidget_ = new QListWidget(leftWidget);
    connect(weaponListWidget_, &QListWidget::currentRowChanged, this, &MainWindow::onWeaponSelected);
    leftLayout->addWidget(weaponListWidget_, 1);

    // Right panel: editor + summary table
    auto* rightWidget = new QWidget(splitter);
    auto* rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->setContentsMargins(4, 0, 0, 0);
    rightLayout->setSpacing(8);

    auto* editGroup = new QGroupBox(QStringLiteral("Weapon Skin Configuration"), rightWidget);
    auto* editLayout = new QGridLayout(editGroup);
    editLayout->setSpacing(8);

    weaponTitleLabel_ = new QLabel(QStringLiteral("Select a weapon on the left"), editGroup);
    weaponTitleLabel_->setStyleSheet(QStringLiteral("font-size: 14px; font-weight: bold; color: #61afef;"));
    editLayout->addWidget(weaponTitleLabel_, 0, 0, 1, 3);

    weaponEnableCheck_ = new QCheckBox(QStringLiteral("Enable custom skin for this weapon"), editGroup);
    editLayout->addWidget(weaponEnableCheck_, 1, 0, 1, 3);

    editLayout->addWidget(new QLabel(QStringLiteral("Skin / Finish:"), editGroup), 2, 0);
    weaponSkinCombo_ = new QComboBox(editGroup);
    connect(weaponSkinCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onWeaponSkinComboChanged);
    editLayout->addWidget(weaponSkinCombo_, 2, 1, 1, 2);

    editLayout->addWidget(new QLabel(QStringLiteral("Paint Kit ID:"), editGroup), 3, 0);
    weaponPaintKitSpin_ = new QSpinBox(editGroup);
    weaponPaintKitSpin_->setRange(0, 99999);
    editLayout->addWidget(weaponPaintKitSpin_, 3, 1, 1, 2);

    editLayout->addWidget(new QLabel(QStringLiteral("Wear Preset:"), editGroup), 4, 0);
    weaponWearPresetCombo_ = new QComboBox(editGroup);
    weaponWearPresetCombo_->addItems({
        QStringLiteral("Factory New (0.01)"),
        QStringLiteral("Minimal Wear (0.08)"),
        QStringLiteral("Field-Tested (0.20)"),
        QStringLiteral("Well-Worn (0.40)"),
        QStringLiteral("Battle-Scarred (0.70)"),
        QStringLiteral("Custom...")
    });
    connect(weaponWearPresetCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onWeaponWearPresetChanged);
    editLayout->addWidget(weaponWearPresetCombo_, 4, 1, 1, 2);

    editLayout->addWidget(new QLabel(QStringLiteral("Wear Value:"), editGroup), 5, 0);
    weaponWearSpin_ = new QDoubleSpinBox(editGroup);
    weaponWearSpin_->setRange(0.0001, 1.0000);
    weaponWearSpin_->setDecimals(4);
    weaponWearSpin_->setSingleStep(0.01);
    weaponWearSpin_->setValue(0.0100);
    editLayout->addWidget(weaponWearSpin_, 5, 1, 1, 2);

    editLayout->addWidget(new QLabel(QStringLiteral("Pattern Seed:"), editGroup), 6, 0);
    weaponSeedSpin_ = new QSpinBox(editGroup);
    weaponSeedSpin_->setRange(1, 1000);
    weaponSeedSpin_->setValue(1);
    editLayout->addWidget(weaponSeedSpin_, 6, 1, 1, 2);

    auto* btnRow = new QHBoxLayout();
    weaponSaveBtn_ = new QPushButton(QStringLiteral("✔ Save Weapon Skin"), editGroup);
    weaponSaveBtn_->setStyleSheet(QStringLiteral("font-weight: bold; background-color: #2b5278; color: white;"));
    connect(weaponSaveBtn_, &QPushButton::clicked, this, &MainWindow::onSaveWeaponClicked);
    btnRow->addWidget(weaponSaveBtn_);

    weaponClearBtn_ = new QPushButton(QStringLiteral("✖ Remove Custom Skin"), editGroup);
    connect(weaponClearBtn_, &QPushButton::clicked, this, &MainWindow::onClearWeaponClicked);
    btnRow->addWidget(weaponClearBtn_);

    editLayout->addLayout(btnRow, 7, 0, 1, 3);
    rightLayout->addWidget(editGroup);

    // Summary table
    auto* tableGroup = new QGroupBox(QStringLiteral("Configured Weapons Overview"), rightWidget);
    auto* tableLayout = new QVBoxLayout(tableGroup);
    tableLayout->setContentsMargins(4, 6, 4, 4);

    weaponSummaryTable_ = new QTableWidget(0, 5, tableGroup);
    weaponSummaryTable_->setHorizontalHeaderLabels({
        QStringLiteral("Weapon"),
        QStringLiteral("Skin / Finish"),
        QStringLiteral("Paint Kit"),
        QStringLiteral("Wear"),
        QStringLiteral("Seed")
    });
    weaponSummaryTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    weaponSummaryTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    weaponSummaryTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    weaponSummaryTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    weaponSummaryTable_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    weaponSummaryTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    weaponSummaryTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connect(weaponSummaryTable_, &QTableWidget::cellDoubleClicked,
            this, &MainWindow::onWeaponTableCellDoubleClicked);

    tableLayout->addWidget(weaponSummaryTable_);

    auto* tableBtnRow = new QHBoxLayout();
    tableBtnRow->addStretch();
    auto* resetAllWeaponsBtn = new QPushButton(QStringLiteral("🗑 Reset All Weapons"), tableGroup);
    resetAllWeaponsBtn->setToolTip(QStringLiteral("Remove all configured custom weapon skins"));
    connect(resetAllWeaponsBtn, &QPushButton::clicked, this, &MainWindow::onResetAllWeaponsClicked);
    tableBtnRow->addWidget(resetAllWeaponsBtn);
    tableLayout->addLayout(tableBtnRow);

    rightLayout->addWidget(tableGroup, 1);

    splitter->addWidget(leftWidget);
    splitter->addWidget(rightWidget);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);

    tabLayout->addWidget(splitter);
    tabWidget_->addTab(tab, QStringLiteral("🔫 Weapons & Skins"));
}

void MainWindow::setupKnivesTab() {
    auto* tab = new QWidget(this);
    auto* tabLayout = new QVBoxLayout(tab);
    tabLayout->setContentsMargins(12, 12, 12, 12);
    tabLayout->setSpacing(12);

    auto* colsLayout = new QHBoxLayout();
    colsLayout->setSpacing(12);

    // CT Knife Group
    auto* ctGroup = new QGroupBox(QStringLiteral("Counter-Terrorist (CT) Knife"), tab);
    auto* ctLayout = new QFormLayout(ctGroup);
    ctLayout->setSpacing(8);

    knifeCtEnableCheck_ = new QCheckBox(QStringLiteral("Override CT Knife"), ctGroup);
    ctLayout->addRow(knifeCtEnableCheck_);

    knifeCtModelCombo_ = new QComboBox(ctGroup);
    ctLayout->addRow(QStringLiteral("Model:"), knifeCtModelCombo_);

    knifeCtSkinCombo_ = new QComboBox(ctGroup);
    connect(knifeCtSkinCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onKnifeCtSkinComboChanged);
    ctLayout->addRow(QStringLiteral("Skin:"), knifeCtSkinCombo_);

    knifeCtPaintKitSpin_ = new QSpinBox(ctGroup);
    knifeCtPaintKitSpin_->setRange(0, 99999);
    ctLayout->addRow(QStringLiteral("Paint Kit ID:"), knifeCtPaintKitSpin_);

    knifeCtWearSpin_ = new QDoubleSpinBox(ctGroup);
    knifeCtWearSpin_->setRange(0.0001, 1.0000);
    knifeCtWearSpin_->setDecimals(4);
    knifeCtWearSpin_->setSingleStep(0.01);
    knifeCtWearSpin_->setValue(0.0100);
    ctLayout->addRow(QStringLiteral("Wear:"), knifeCtWearSpin_);

    knifeCtSeedSpin_ = new QSpinBox(ctGroup);
    knifeCtSeedSpin_->setRange(1, 1000);
    knifeCtSeedSpin_->setValue(1);
    ctLayout->addRow(QStringLiteral("Seed:"), knifeCtSeedSpin_);

    colsLayout->addWidget(ctGroup);

    // T Knife Group
    auto* tGroup = new QGroupBox(QStringLiteral("Terrorist (T) Knife"), tab);
    auto* tLayout = new QFormLayout(tGroup);
    tLayout->setSpacing(8);

    knifeTEnableCheck_ = new QCheckBox(QStringLiteral("Override T Knife"), tGroup);
    tLayout->addRow(knifeTEnableCheck_);

    knifeTModelCombo_ = new QComboBox(tGroup);
    tLayout->addRow(QStringLiteral("Model:"), knifeTModelCombo_);

    knifeTSkinCombo_ = new QComboBox(tGroup);
    connect(knifeTSkinCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onKnifeTSkinComboChanged);
    tLayout->addRow(QStringLiteral("Skin:"), knifeTSkinCombo_);

    knifeTPaintKitSpin_ = new QSpinBox(tGroup);
    knifeTPaintKitSpin_->setRange(0, 99999);
    tLayout->addRow(QStringLiteral("Paint Kit ID:"), knifeTPaintKitSpin_);

    knifeTWearSpin_ = new QDoubleSpinBox(tGroup);
    knifeTWearSpin_->setRange(0.0001, 1.0000);
    knifeTWearSpin_->setDecimals(4);
    knifeTWearSpin_->setSingleStep(0.01);
    knifeTWearSpin_->setValue(0.0100);
    tLayout->addRow(QStringLiteral("Wear:"), knifeTWearSpin_);

    knifeTSeedSpin_ = new QSpinBox(tGroup);
    knifeTSeedSpin_->setRange(1, 1000);
    knifeTSeedSpin_->setValue(1);
    tLayout->addRow(QStringLiteral("Seed:"), knifeTSeedSpin_);

    colsLayout->addWidget(tGroup);
    tabLayout->addLayout(colsLayout);

    auto markKnifeDirty = [this]() {
        if (!updatingUi_) {
            setDirty(true);
            rawJsonEdit_->setPlainText(QString::fromUtf8(buildConfigJson()));
        }
    };
    connect(knifeCtEnableCheck_, &QCheckBox::toggled, this, markKnifeDirty);
    connect(knifeCtModelCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, markKnifeDirty);
    connect(knifeCtSkinCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, markKnifeDirty);
    connect(knifeCtPaintKitSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, markKnifeDirty);
    connect(knifeCtWearSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, markKnifeDirty);
    connect(knifeCtSeedSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, markKnifeDirty);

    connect(knifeTEnableCheck_, &QCheckBox::toggled, this, markKnifeDirty);
    connect(knifeTModelCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, markKnifeDirty);
    connect(knifeTSkinCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, markKnifeDirty);
    connect(knifeTPaintKitSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, markKnifeDirty);
    connect(knifeTWearSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, markKnifeDirty);
    connect(knifeTSeedSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, markKnifeDirty);

    // Utility buttons
    auto* btnRow = new QHBoxLayout();
    btnRow->addStretch();

    auto* copyCtToT = new QPushButton(QStringLiteral("➡ Copy CT Knife to T"), tab);
    connect(copyCtToT, &QPushButton::clicked, this, &MainWindow::onCopyCtToTClicked);
    btnRow->addWidget(copyCtToT);

    auto* copyTToCt = new QPushButton(QStringLiteral("⬅ Copy T Knife to CT"), tab);
    connect(copyTToCt, &QPushButton::clicked, this, &MainWindow::onCopyTToCtClicked);
    btnRow->addWidget(copyTToCt);

    btnRow->addStretch();
    tabLayout->addLayout(btnRow);
    tabLayout->addStretch();

    tabWidget_->addTab(tab, QStringLiteral("🗡 Knives"));
}

void MainWindow::setupAgentsTab() {
    auto* tab = new QWidget(this);
    auto* tabLayout = new QVBoxLayout(tab);
    tabLayout->setContentsMargins(16, 16, 16, 16);
    tabLayout->setSpacing(14);

    agentEnableCheck_ = new QCheckBox(QStringLiteral("Enable Custom Player Agents"), tab);
    agentEnableCheck_->setStyleSheet(QStringLiteral("font-size: 14px; font-weight: bold;"));
    tabLayout->addWidget(agentEnableCheck_);

    auto* ctGroup = new QGroupBox(QStringLiteral("Counter-Terrorist (CT) Agent"), tab);
    auto* ctLayout = new QVBoxLayout(ctGroup);
    agentCtCombo_ = new QComboBox(ctGroup);
    ctLayout->addWidget(agentCtCombo_);
    tabLayout->addWidget(ctGroup);

    auto* tGroup = new QGroupBox(QStringLiteral("Terrorist (T) Agent"), tab);
    auto* tLayout = new QVBoxLayout(tGroup);
    agentTCombo_ = new QComboBox(tGroup);
    tLayout->addWidget(agentTCombo_);
    tabLayout->addWidget(tGroup);

    auto markAgentDirty = [this]() {
        if (!updatingUi_) {
            setDirty(true);
            rawJsonEdit_->setPlainText(QString::fromUtf8(buildConfigJson()));
        }
    };
    connect(agentEnableCheck_, &QCheckBox::toggled, this, markAgentDirty);
    connect(agentCtCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, markAgentDirty);
    connect(agentTCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, markAgentDirty);

    auto* noteLabel = new QLabel(
        QStringLiteral("ℹ Custom agent models are applied to your player pawn during Freezetime / Round Start and dynamically hot-swapped."),
        tab);
    noteLabel->setStyleSheet(QStringLiteral("color: #abb2bf; font-style: italic;"));
    tabLayout->addWidget(noteLabel);

    tabLayout->addStretch();
    tabWidget_->addTab(tab, QStringLiteral("👤 Agents"));
}

void MainWindow::setupSettingsTab() {
    auto* tab = new QWidget(this);
    auto* tabLayout = new QVBoxLayout(tab);
    tabLayout->setContentsMargins(10, 10, 10, 10);
    tabLayout->setSpacing(10);

    auto* infoGroup = new QGroupBox(QStringLiteral("System & IPC Information"), tab);
    auto* infoLayout = new QFormLayout(infoGroup);

    configPathLabel_ = new QLabel(IpcClient::defaultConfigFile(), infoGroup);
    configPathLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    infoLayout->addRow(QStringLiteral("Config File:"), configPathLabel_);

    socketPathLabel_ = new QLabel(ipcClient_->socketPath(), infoGroup);
    socketPathLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    infoLayout->addRow(QStringLiteral("IPC Socket:"), socketPathLabel_);

    tabLayout->addWidget(infoGroup);

    auto* jsonGroup = new QGroupBox(QStringLiteral("Raw Configuration JSON (V2)"), tab);
    auto* jsonLayout = new QVBoxLayout(jsonGroup);

    rawJsonEdit_ = new QTextEdit(jsonGroup);
    rawJsonEdit_->setFont(QFont(QStringLiteral("Monospace"), 10));
    rawJsonEdit_->setStyleSheet(QStringLiteral("background-color: #1e1e1e; color: #d4d4d4;"));
    jsonLayout->addWidget(rawJsonEdit_);

    auto* jsonBtnRow = new QHBoxLayout();
    auto* applyRawBtn = new QPushButton(QStringLiteral("Apply Raw JSON"), jsonGroup);
    connect(applyRawBtn, &QPushButton::clicked, this, &MainWindow::onApplyRawJsonClicked);
    jsonBtnRow->addWidget(applyRawBtn);

    auto* formatBtn = new QPushButton(QStringLiteral("Format / Prettify JSON"), jsonGroup);
    connect(formatBtn, &QPushButton::clicked, this, &MainWindow::onFormatRawJsonClicked);
    jsonBtnRow->addWidget(formatBtn);

    auto* resetBtn = new QPushButton(QStringLiteral("Reset to Defaults"), jsonGroup);
    connect(resetBtn, &QPushButton::clicked, this, &MainWindow::onResetDefaultsClicked);
    jsonBtnRow->addWidget(resetBtn);

    jsonLayout->addLayout(jsonBtnRow);
    tabLayout->addWidget(jsonGroup, 1);

    tabWidget_->addTab(tab, QStringLiteral("⚙ Raw JSON & Info"));
}

static bool IsWeaponInCategory(const QString& id, int categoryIndex) {
    if (categoryIndex == 0) return true; // All

    // Category 1: Rifles
    if (categoryIndex == 1) {
        return (id == "ak47" || id == "m4a1" || id == "m4a1_silencer" ||
                id == "famas" || id == "galil" || id == "aug" || id == "sg553");
    }
    // Category 2: Pistols
    if (categoryIndex == 2) {
        return (id == "deagle" || id == "glock" || id == "usp_silencer" ||
                id == "p2000" || id == "p250" || id == "five_seven" ||
                id == "tec9" || id == "cz75" || id == "dual_berettas" || id == "revolver");
    }
    // Category 3: Snipers
    if (categoryIndex == 3) {
        return (id == "awp" || id == "ssg08" || id == "scar20" || id == "g3sg1");
    }
    // Category 4: SMGs
    if (categoryIndex == 4) {
        return (id == "mp9" || id == "mac10" || id == "mp7" ||
                id == "mp5sd" || id == "ump45" || id == "p90" || id == "bizon");
    }
    // Category 5: Heavy / Shotguns
    if (categoryIndex == 5) {
        return (id == "nova" || id == "xm1014" || id == "mag7" ||
                id == "sawedoff" || id == "m249" || id == "negev");
    }
    return true;
}

void MainWindow::populateCatalogs() {
    // Populate weapons list
    weaponListWidget_->clear();
    for (std::size_t i = 0; i < itemcatalog::WeaponCount; ++i) {
        const auto& w = itemcatalog::Weapons[i];
        auto* item = new QListWidgetItem(QStringLiteral("%1 (%2)")
                                             .arg(QString::fromUtf8(w.displayName))
                                             .arg(QString::fromUtf8(w.canonicalName)));
        item->setData(Qt::UserRole, QString::fromUtf8(w.canonicalName));
        item->setData(Qt::UserRole + 1, static_cast<uint>(w.defIndex));
        item->setData(Qt::UserRole + 2, QString::fromUtf8(w.displayName));
        weaponListWidget_->addItem(item);
    }

    // Populate knife models
    knifeCtModelCombo_->clear();
    knifeTModelCombo_->clear();
    for (std::size_t i = 0; i < itemcatalog::KnifeCount; ++i) {
        const auto& k = itemcatalog::Knives[i];
        QString title = QString::fromUtf8(k.name);
        title[0] = title[0].toUpper();
        QString text = QStringLiteral("%1 [Def: %2]").arg(title).arg(k.defIndex);
        knifeCtModelCombo_->addItem(text, QString::fromUtf8(k.name));
        knifeTModelCombo_->addItem(text, QString::fromUtf8(k.name));
    }

    // Populate knife skins
    knifeCtSkinCombo_->clear();
    knifeTSkinCombo_->clear();
    auto knifeSkins = itemcatalog::GetKnifeSkins();
    for (const auto& s : knifeSkins) {
        QString text = QStringLiteral("%1 (ID %2)").arg(QString::fromUtf8(s.displayName)).arg(s.finishId);
        knifeCtSkinCombo_->addItem(text, s.finishId);
        knifeTSkinCombo_->addItem(text, s.finishId);
    }
    knifeCtSkinCombo_->addItem(QStringLiteral("Custom Paint Kit ID..."), -1);
    knifeTSkinCombo_->addItem(QStringLiteral("Custom Paint Kit ID..."), -1);

    // Populate agents
    agentCtCombo_->clear();
    agentCtCombo_->addItem(QStringLiteral("Default CT Agent (No override)"), 0);

    agentTCombo_->clear();
    agentTCombo_->addItem(QStringLiteral("Default T Agent (No override)"), 0);

    for (std::size_t i = 0; i < itemcatalog::AgentCount; ++i) {
        const auto& a = itemcatalog::Agents[i];
        QString text = QStringLiteral("%1 [Def: %2]").arg(QString::fromUtf8(a.name)).arg(a.def);
        if (a.team == 3) {
            agentCtCombo_->addItem(text, a.def);
        } else if (a.team == 2) {
            agentTCombo_->addItem(text, a.def);
        } else {
            agentCtCombo_->addItem(text, a.def);
            agentTCombo_->addItem(text, a.def);
        }
    }

    // Select first weapon
    if (weaponListWidget_->count() > 0) {
        weaponListWidget_->setCurrentRow(0);
    }
}

void MainWindow::onConnectionStatusChanged(bool connected, quint64 version) {
    QString timeStr = QTime::currentTime().toString(QStringLiteral("hh:mm:ss"));
    if (connected) {
        statusBadgeLabel_->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 13px; color: #98c379;"));
        statusBadgeLabel_->setText(QStringLiteral("● CS2 Runtime: Connected (v%1) [%2] - Live Hot-Reload Active").arg(version).arg(timeStr));
        pullBtn_->setEnabled(true);
    } else {
        statusBadgeLabel_->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 13px; color: #e5c07b;"));
        statusBadgeLabel_->setText(QStringLiteral("○ CS2 Runtime: Not Running [%1] (Offline Mode - Changes save to disk)").arg(timeStr));
        pullBtn_->setEnabled(false);
    }
}

void MainWindow::onConfigReceived(const QByteArray& json, quint64 version) {
    loadConfigFromJson(json);
    statusBar()->showMessage(QStringLiteral("Pulled active config from CS2 runtime (Snapshot v%1)").arg(version), 4000);
}

void MainWindow::onConfigApplied(bool success, const QString& message, quint64 version) {
    Q_UNUSED(version);
    if (success) {
        statusBar()->showMessage(message, 5000);
    } else {
        QMessageBox::warning(this, QStringLiteral("Apply Config Notice"), message);
    }
}

void MainWindow::onApplyClicked() {
    QByteArray payload = buildConfigJson();
    quint64 version = 0;
    QString err;
    bool ok = ipcClient_->applyConfig(payload, &version, &err);
    if (ok) {
        setDirty(false);
        if (ipcClient_->isConnected()) {
            statusBar()->showMessage(QStringLiteral("Applied live to CS2! (Runtime snapshot v%1)").arg(version), 5000);
        } else {
            statusBar()->showMessage(QStringLiteral("Config saved to disk! CS2 will load it on startup."), 5000);
        }
    } else {
        statusBar()->showMessage(QStringLiteral("Error: ") + err, 5000);
    }
}

void MainWindow::onSaveClicked() {
    QByteArray payload = buildConfigJson();
    if (IpcClient::saveToDisk(payload)) {
        setDirty(false);
        statusBar()->showMessage(QStringLiteral("Saved config to ") + IpcClient::defaultConfigFile(), 4000);
    } else {
        QMessageBox::critical(this, QStringLiteral("Save Error"), QStringLiteral("Failed to write config file to disk."));
    }
}

void MainWindow::onReloadClicked() {
    QByteArray diskJson;
    if (IpcClient::loadFromDisk(diskJson)) {
        loadConfigFromJson(diskJson);
        statusBar()->showMessage(QStringLiteral("Reloaded config from disk."), 3000);
    } else {
        QMessageBox::warning(this, QStringLiteral("Reload"), QStringLiteral("Config file does not exist yet."));
    }
}

void MainWindow::onPullFromGameClicked() {
    if (!ipcClient_->isConnected()) {
        QMessageBox::information(this, QStringLiteral("Pull from Game"), QStringLiteral("CS2 runtime is not currently running."));
        return;
    }
    QByteArray remoteJson;
    quint64 version = 0;
    if (ipcClient_->getConfig(remoteJson, &version)) {
        loadConfigFromJson(remoteJson);
    }
}

void MainWindow::onResetDefaultsClicked() {
    // Standard V2 clean template
    const char* defaultJson = R"({
  "config_version": 2,
  "knives": {
    "ct": {
      "model": "karambit",
      "skin": "doppler",
      "paint_kit": 415,
      "wear": 0.01,
      "seed": 1
    },
    "t": {
      "model": "butterfly",
      "skin": "fade",
      "paint_kit": 38,
      "wear": 0.01,
      "seed": 1
    }
  },
  "agents": {
    "ct": "wet_sox",
    "t": "darryl_royale"
  },
  "weapons": {
    "ak47": {
      "skin": "printstream",
      "paint_kit": 1242,
      "wear": 0.01,
      "seed": 1
    },
    "awp": {
      "skin": "asiimov",
      "paint_kit": 279,
      "wear": 0.18,
      "seed": 1
    },
    "deagle": {
      "skin": "printstream",
      "paint_kit": 1007,
      "wear": 0.01,
      "seed": 1
    },
    "m4a1_silencer": {
      "skin": "printstream",
      "paint_kit": 984,
      "wear": 0.01,
      "seed": 1
    }
  }
})";
    loadConfigFromJson(QByteArray(defaultJson));
    setDirty(true);
    statusBar()->showMessage(QStringLiteral("Reset to default configuration template. Click 'Save' or 'Apply' to persist."), 4000);
}

void MainWindow::onWeaponSearchChanged(const QString& text) {
    int cat = weaponCategoryCombo_->currentIndex();
    QString query = text.trimmed().toLower();

    for (int i = 0; i < weaponListWidget_->count(); ++i) {
        auto* item = weaponListWidget_->item(i);
        QString wId = item->data(Qt::UserRole).toString();
        QString wDisp = item->data(Qt::UserRole + 2).toString().toLower();

        bool matchCat = IsWeaponInCategory(wId, cat);
        bool matchText = query.isEmpty() || wId.contains(query) || wDisp.contains(query);
        item->setHidden(!matchCat || !matchText);
    }
}

void MainWindow::onWeaponCategoryChanged(int index) {
    Q_UNUSED(index);
    onWeaponSearchChanged(weaponSearchEdit_->text());
}

void MainWindow::onWeaponSelected(int index) {
    if (index < 0 || index >= weaponListWidget_->count())
        return;
    auto* item = weaponListWidget_->item(index);
    if (!item) return;

    QString wId = item->data(Qt::UserRole).toString();
    currentSelectedWeaponId_ = wId;
    updateWeaponEditor(wId);
}

void MainWindow::updateWeaponEditor(const QString& weaponId) {
    const auto* wInfo = itemcatalog::FindWeapon(weaponId.toStdString());
    QString disp = wInfo ? QString::fromUtf8(wInfo->displayName) : weaponId;
    uint def = wInfo ? wInfo->defIndex : 0;
    weaponTitleLabel_->setText(QStringLiteral("Configuring: %1 [Def: %2]").arg(disp).arg(def));

    // Populate skins for this weapon
    weaponSkinCombo_->blockSignals(true);
    weaponSkinCombo_->clear();

    auto skins = itemcatalog::GetSkinsForWeapon(weaponId.toStdString());
    for (const auto& s : skins) {
        QString text = QStringLiteral("%1 (ID %2)").arg(QString::fromUtf8(s.displayName)).arg(s.finishId);
        weaponSkinCombo_->addItem(text, s.finishId);
    }
    weaponSkinCombo_->addItem(QStringLiteral("Custom Paint Kit ID..."), -1);
    weaponSkinCombo_->blockSignals(false);

    // Read current weapon setting from config
    QJsonObject weaponsObj = currentConfig_.value(QStringLiteral("weapons")).toObject();
    if (weaponsObj.contains(weaponId)) {
        QJsonObject wObj = weaponsObj.value(weaponId).toObject();
        weaponEnableCheck_->setChecked(true);

        int pKit = wObj.value(QStringLiteral("paint_kit")).toInt(0);
        weaponPaintKitSpin_->setValue(pKit);

        // Find skin in combo
        int foundIdx = -1;
        for (int i = 0; i < weaponSkinCombo_->count(); ++i) {
            if (weaponSkinCombo_->itemData(i).toInt() == pKit) {
                foundIdx = i;
                break;
            }
        }
        if (foundIdx >= 0) {
            weaponSkinCombo_->setCurrentIndex(foundIdx);
        } else {
            weaponSkinCombo_->setCurrentIndex(weaponSkinCombo_->count() - 1); // Custom
        }

        double wear = wObj.value(QStringLiteral("wear")).toDouble(0.0100);
        weaponWearSpin_->setValue(wear);

        int seed = wObj.value(QStringLiteral("seed")).toInt(1);
        weaponSeedSpin_->setValue(seed);
    } else {
        weaponEnableCheck_->setChecked(false);
        weaponSkinCombo_->setCurrentIndex(0);
        int pKit = weaponSkinCombo_->currentData().toInt();
        if (pKit >= 0) weaponPaintKitSpin_->setValue(pKit);
        weaponWearSpin_->setValue(0.0100);
        weaponSeedSpin_->setValue(1);
    }
}

void MainWindow::onWeaponSkinComboChanged(int index) {
    if (index < 0) return;
    int pKit = weaponSkinCombo_->itemData(index).toInt();
    if (pKit >= 0) {
        weaponPaintKitSpin_->setValue(pKit);
    }
}

void MainWindow::onWeaponWearPresetChanged(int index) {
    switch (index) {
        case 0: weaponWearSpin_->setValue(0.0100); break; // Factory New
        case 1: weaponWearSpin_->setValue(0.0800); break; // Minimal Wear
        case 2: weaponWearSpin_->setValue(0.2000); break; // Field-Tested
        case 3: weaponWearSpin_->setValue(0.4000); break; // Well-Worn
        case 4: weaponWearSpin_->setValue(0.7000); break; // Battle-Scarred
        default: break;
    }
}

void MainWindow::onSaveWeaponClicked() {
    if (currentSelectedWeaponId_.isEmpty())
        return;

    QJsonObject weaponsObj = currentConfig_.value(QStringLiteral("weapons")).toObject();

    if (weaponEnableCheck_->isChecked()) {
        QJsonObject wObj;
        int pKit = weaponPaintKitSpin_->value();
        wObj[QStringLiteral("paint_kit")] = pKit;
        wObj[QStringLiteral("wear")] = weaponWearSpin_->value();
        wObj[QStringLiteral("seed")] = weaponSeedSpin_->value();

        // Include skin idName if matched
        int skinIdx = weaponSkinCombo_->currentIndex();
        if (skinIdx >= 0 && skinIdx < weaponSkinCombo_->count() - 1) {
            auto skins = itemcatalog::GetSkinsForWeapon(currentSelectedWeaponId_.toStdString());
            if (static_cast<std::size_t>(skinIdx) < skins.size()) {
                wObj[QStringLiteral("skin")] = QString::fromUtf8(skins[skinIdx].idName);
            }
        }

        weaponsObj[currentSelectedWeaponId_] = wObj;
        statusBar()->showMessage(QStringLiteral("Saved skin for %1").arg(currentSelectedWeaponId_), 2500);
    } else {
        weaponsObj.remove(currentSelectedWeaponId_);
        statusBar()->showMessage(QStringLiteral("Disabled custom skin for %1").arg(currentSelectedWeaponId_), 2500);
    }

    currentConfig_[QStringLiteral("weapons")] = weaponsObj;
    refreshWeaponTable();
    rawJsonEdit_->setPlainText(QString::fromUtf8(buildConfigJson()));
    setDirty(true);
}

void MainWindow::onClearWeaponClicked() {
    if (currentSelectedWeaponId_.isEmpty())
        return;

    weaponEnableCheck_->setChecked(false);
    onSaveWeaponClicked();
}

void MainWindow::onWeaponTableCellDoubleClicked(int row, int column) {
    Q_UNUSED(column);
    if (row < 0 || row >= weaponSummaryTable_->rowCount())
        return;
    auto* item = weaponSummaryTable_->item(row, 0);
    if (!item) return;

    QString wId = item->data(Qt::UserRole).toString();
    for (int i = 0; i < weaponListWidget_->count(); ++i) {
        if (weaponListWidget_->item(i)->data(Qt::UserRole).toString() == wId) {
            weaponListWidget_->setCurrentRow(i);
            break;
        }
    }
}

void MainWindow::refreshWeaponTable() {
    weaponSummaryTable_->setRowCount(0);
    QJsonObject weaponsObj = currentConfig_.value(QStringLiteral("weapons")).toObject();

    for (auto it = weaponsObj.begin(); it != weaponsObj.end(); ++it) {
        QString wId = it.key();
        QJsonObject wData = it.value().toObject();

        const auto* wInfo = itemcatalog::FindWeapon(wId.toStdString());
        QString dispName = wInfo ? QString::fromUtf8(wInfo->displayName) : wId;

        int row = weaponSummaryTable_->rowCount();
        weaponSummaryTable_->insertRow(row);

        auto* itemWeapon = new QTableWidgetItem(dispName);
        itemWeapon->setData(Qt::UserRole, wId);
        weaponSummaryTable_->setItem(row, 0, itemWeapon);

        QString skinStr = wData.value(QStringLiteral("skin")).toString();
        if (skinStr.isEmpty()) skinStr = QStringLiteral("Custom ID");
        weaponSummaryTable_->setItem(row, 1, new QTableWidgetItem(skinStr));

        int pKit = wData.value(QStringLiteral("paint_kit")).toInt(0);
        weaponSummaryTable_->setItem(row, 2, new QTableWidgetItem(QString::number(pKit)));

        double wear = wData.value(QStringLiteral("wear")).toDouble(0.0100);
        weaponSummaryTable_->setItem(row, 3, new QTableWidgetItem(QString::number(wear, 'f', 4)));

        int seed = wData.value(QStringLiteral("seed")).toInt(1);
        weaponSummaryTable_->setItem(row, 4, new QTableWidgetItem(QString::number(seed)));
    }
}

void MainWindow::onKnifeCtSkinComboChanged(int index) {
    if (index < 0) return;
    int pKit = knifeCtSkinCombo_->itemData(index).toInt();
    if (pKit >= 0) {
        knifeCtPaintKitSpin_->setValue(pKit);
    }
}

void MainWindow::onKnifeTSkinComboChanged(int index) {
    if (index < 0) return;
    int pKit = knifeTSkinCombo_->itemData(index).toInt();
    if (pKit >= 0) {
        knifeTPaintKitSpin_->setValue(pKit);
    }
}

void MainWindow::onCopyCtToTClicked() {
    knifeTEnableCheck_->setChecked(knifeCtEnableCheck_->isChecked());
    knifeTModelCombo_->setCurrentIndex(knifeCtModelCombo_->currentIndex());
    knifeTSkinCombo_->setCurrentIndex(knifeCtSkinCombo_->currentIndex());
    knifeTPaintKitSpin_->setValue(knifeCtPaintKitSpin_->value());
    knifeTWearSpin_->setValue(knifeCtWearSpin_->value());
    knifeTSeedSpin_->setValue(knifeCtSeedSpin_->value());
    statusBar()->showMessage(QStringLiteral("Copied CT Knife settings to T Knife"), 2500);
    setDirty(true);
    rawJsonEdit_->setPlainText(QString::fromUtf8(buildConfigJson()));
}

void MainWindow::onCopyTToCtClicked() {
    knifeCtEnableCheck_->setChecked(knifeTEnableCheck_->isChecked());
    knifeCtModelCombo_->setCurrentIndex(knifeTModelCombo_->currentIndex());
    knifeCtSkinCombo_->setCurrentIndex(knifeTSkinCombo_->currentIndex());
    knifeCtPaintKitSpin_->setValue(knifeTPaintKitSpin_->value());
    knifeCtWearSpin_->setValue(knifeTWearSpin_->value());
    knifeCtSeedSpin_->setValue(knifeTSeedSpin_->value());
    statusBar()->showMessage(QStringLiteral("Copied T Knife settings to CT Knife"), 2500);
    setDirty(true);
    rawJsonEdit_->setPlainText(QString::fromUtf8(buildConfigJson()));
}

void MainWindow::onApplyRawJsonClicked() {
    QByteArray raw = rawJsonEdit_->toPlainText().toUtf8();
    QJsonParseError err{};
    QJsonDocument doc = QJsonDocument::fromJson(raw, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        QMessageBox::critical(this, QStringLiteral("JSON Syntax Error"),
                              QStringLiteral("Invalid JSON: ") + err.errorString());
        return;
    }

    loadConfigFromJson(raw);
    onApplyClicked();
}

void MainWindow::onFormatRawJsonClicked() {
    QByteArray raw = rawJsonEdit_->toPlainText().toUtf8();
    QJsonParseError err{};
    QJsonDocument doc = QJsonDocument::fromJson(raw, &err);
    if (err.error != QJsonParseError::NoError) {
        QMessageBox::warning(this, QStringLiteral("JSON Error"),
                             QStringLiteral("Cannot format invalid JSON: ") + err.errorString());
        return;
    }
    rawJsonEdit_->setPlainText(QString::fromUtf8(doc.toJson(QJsonDocument::Indented)));
}

void MainWindow::loadConfigFromJson(const QByteArray& jsonBytes) {
    QJsonParseError parseErr{};
    QJsonDocument doc = QJsonDocument::fromJson(jsonBytes, &parseErr);
    if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) {
        return;
    }

    currentConfig_ = doc.object();
    refreshUiFromConfig();
    setDirty(false);
}

void MainWindow::refreshUiFromConfig() {
    updatingUi_ = true;

    // 1. Knives
    QJsonObject knivesObj = currentConfig_.value(QStringLiteral("knives")).toObject();
    QJsonObject ctKnife = knivesObj.value(QStringLiteral("ct")).toObject();
    QJsonObject tKnife = knivesObj.value(QStringLiteral("t")).toObject();

    if (!ctKnife.isEmpty() || knivesObj.contains(QStringLiteral("model"))) {
        knifeCtEnableCheck_->setChecked(true);
        QString m = ctKnife.value(QStringLiteral("model")).toString();
        if (m.isEmpty()) m = knivesObj.value(QStringLiteral("model")).toString();
        int mIdx = knifeCtModelCombo_->findData(m);
        if (mIdx >= 0) knifeCtModelCombo_->setCurrentIndex(mIdx);

        int pKit = ctKnife.value(QStringLiteral("paint_kit")).toInt(0);
        knifeCtPaintKitSpin_->setValue(pKit);
        int sIdx = knifeCtSkinCombo_->findData(pKit);
        if (sIdx >= 0) knifeCtSkinCombo_->setCurrentIndex(sIdx);
        else knifeCtSkinCombo_->setCurrentIndex(knifeCtSkinCombo_->count() - 1);

        knifeCtWearSpin_->setValue(ctKnife.value(QStringLiteral("wear")).toDouble(0.0100));
        knifeCtSeedSpin_->setValue(ctKnife.value(QStringLiteral("seed")).toInt(1));
    } else {
        knifeCtEnableCheck_->setChecked(false);
    }

    if (!tKnife.isEmpty() || knivesObj.contains(QStringLiteral("model"))) {
        knifeTEnableCheck_->setChecked(true);
        QString m = tKnife.value(QStringLiteral("model")).toString();
        if (m.isEmpty()) m = knivesObj.value(QStringLiteral("model")).toString();
        int mIdx = knifeTModelCombo_->findData(m);
        if (mIdx >= 0) knifeTModelCombo_->setCurrentIndex(mIdx);

        int pKit = tKnife.value(QStringLiteral("paint_kit")).toInt(0);
        knifeTPaintKitSpin_->setValue(pKit);
        int sIdx = knifeTSkinCombo_->findData(pKit);
        if (sIdx >= 0) knifeTSkinCombo_->setCurrentIndex(sIdx);
        else knifeTSkinCombo_->setCurrentIndex(knifeTSkinCombo_->count() - 1);

        knifeTWearSpin_->setValue(tKnife.value(QStringLiteral("wear")).toDouble(0.0100));
        knifeTSeedSpin_->setValue(tKnife.value(QStringLiteral("seed")).toInt(1));
    } else {
        knifeTEnableCheck_->setChecked(false);
    }

    // 2. Agents
    QJsonObject agentsObj = currentConfig_.value(QStringLiteral("agents")).toObject();
    if (!agentsObj.isEmpty()) {
        agentEnableCheck_->setChecked(true);
        QString ctAgent = agentsObj.value(QStringLiteral("ct")).toString();
        for (int i = 0; i < agentCtCombo_->count(); ++i) {
            if (agentCtCombo_->itemText(i).contains(ctAgent, Qt::CaseInsensitive)) {
                agentCtCombo_->setCurrentIndex(i);
                break;
            }
        }
        QString tAgent = agentsObj.value(QStringLiteral("t")).toString();
        for (int i = 0; i < agentTCombo_->count(); ++i) {
            if (agentTCombo_->itemText(i).contains(tAgent, Qt::CaseInsensitive)) {
                agentTCombo_->setCurrentIndex(i);
                break;
            }
        }
    } else {
        agentEnableCheck_->setChecked(false);
    }

    // 3. Weapons table & editor
    refreshWeaponTable();
    if (!currentSelectedWeaponId_.isEmpty()) {
        updateWeaponEditor(currentSelectedWeaponId_);
    } else if (weaponListWidget_->count() > 0) {
        weaponListWidget_->setCurrentRow(0);
    }

    // 4. Raw JSON tab
    rawJsonEdit_->setPlainText(QString::fromUtf8(buildConfigJson()));

    updatingUi_ = false;
}

QByteArray MainWindow::buildConfigJson() const {
    QJsonObject root;
    root[QStringLiteral("config_version")] = 2;

    // Knives
    QJsonObject knivesObj;
    if (knifeCtEnableCheck_->isChecked()) {
        QJsonObject ct;
        ct[QStringLiteral("model")] = knifeCtModelCombo_->currentData().toString();
        int sIdx = knifeCtSkinCombo_->currentIndex();
        if (sIdx >= 0 && sIdx < knifeCtSkinCombo_->count() - 1) {
            auto kSkins = itemcatalog::GetKnifeSkins();
            if (static_cast<std::size_t>(sIdx) < kSkins.size()) {
                ct[QStringLiteral("skin")] = QString::fromUtf8(kSkins[sIdx].idName);
            }
        }
        ct[QStringLiteral("paint_kit")] = knifeCtPaintKitSpin_->value();
        ct[QStringLiteral("wear")] = knifeCtWearSpin_->value();
        ct[QStringLiteral("seed")] = knifeCtSeedSpin_->value();
        knivesObj[QStringLiteral("ct")] = ct;
    }
    if (knifeTEnableCheck_->isChecked()) {
        QJsonObject t;
        t[QStringLiteral("model")] = knifeTModelCombo_->currentData().toString();
        int sIdx = knifeTSkinCombo_->currentIndex();
        if (sIdx >= 0 && sIdx < knifeTSkinCombo_->count() - 1) {
            auto kSkins = itemcatalog::GetKnifeSkins();
            if (static_cast<std::size_t>(sIdx) < kSkins.size()) {
                t[QStringLiteral("skin")] = QString::fromUtf8(kSkins[sIdx].idName);
            }
        }
        t[QStringLiteral("paint_kit")] = knifeTPaintKitSpin_->value();
        t[QStringLiteral("wear")] = knifeTWearSpin_->value();
        t[QStringLiteral("seed")] = knifeTSeedSpin_->value();
        knivesObj[QStringLiteral("t")] = t;
    }
    if (!knivesObj.isEmpty()) {
        root[QStringLiteral("knives")] = knivesObj;
    }

    // Agents
    if (agentEnableCheck_->isChecked()) {
        QJsonObject agentsObj;
        int ctDef = agentCtCombo_->currentData().toInt();
        if (ctDef > 0) {
            const auto* a = itemcatalog::FindAgentByDef(static_cast<std::uint32_t>(ctDef));
            agentsObj[QStringLiteral("ct")] = a ? QString::fromUtf8(a->name) : QString::number(ctDef);
        }
        int tDef = agentTCombo_->currentData().toInt();
        if (tDef > 0) {
            const auto* a = itemcatalog::FindAgentByDef(static_cast<std::uint32_t>(tDef));
            agentsObj[QStringLiteral("t")] = a ? QString::fromUtf8(a->name) : QString::number(tDef);
        }
        if (!agentsObj.isEmpty()) {
            root[QStringLiteral("agents")] = agentsObj;
        }
    }

    // Weapons (from currentConfig_ weapons map)
    if (currentConfig_.contains(QStringLiteral("weapons"))) {
        root[QStringLiteral("weapons")] = currentConfig_.value(QStringLiteral("weapons")).toObject();
    }

    QJsonDocument doc(root);
    return doc.toJson(QJsonDocument::Indented);
}

void MainWindow::setDirty(bool dirty) {
    isDirty_ = dirty;
    QString title = QStringLiteral("Hamzex Skinchanger - Control Center");
    if (isDirty_) {
        title += QStringLiteral(" * [Unsaved Changes]");
    }
    setWindowTitle(title);
    if (saveBtn_) {
        saveBtn_->setStyleSheet(isDirty_ ? QStringLiteral("font-weight: bold; background-color: #e06c75; color: white;")
                                         : QStringLiteral("font-weight: bold; background-color: #3e4451; color: white;"));
    }
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (isDirty_) {
        auto res = QMessageBox::question(this, QStringLiteral("Unsaved Changes"),
            QStringLiteral("You have unsaved changes in your configuration.\n\nDo you want to save them before exiting?"),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
            QMessageBox::Save);
        if (res == QMessageBox::Save) {
            onSaveClicked();
            event->accept();
        } else if (res == QMessageBox::Discard) {
            event->accept();
        } else {
            event->ignore();
        }
    } else {
        event->accept();
    }
}

void MainWindow::onResetAllWeaponsClicked() {
    auto res = QMessageBox::question(this, QStringLiteral("Reset All Weapons"),
        QStringLiteral("Are you sure you want to remove all configured custom weapon skins?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (res == QMessageBox::Yes) {
        currentConfig_.remove(QStringLiteral("weapons"));
        refreshWeaponTable();
        if (!currentSelectedWeaponId_.isEmpty()) {
            updateWeaponEditor(currentSelectedWeaponId_);
        }
        rawJsonEdit_->setPlainText(QString::fromUtf8(buildConfigJson()));
        setDirty(true);
        statusBar()->showMessage(QStringLiteral("Cleared all custom weapon skins."), 3000);
    }
}

