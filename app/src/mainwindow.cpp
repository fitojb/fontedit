#include "mainwindow.h"
#include "f2b.h"
#include "facewidget.h"
#include "fontfaceviewmodel.h"
#include "command.h"

#include <QGraphicsGridLayout>
#include <QGraphicsWidget>
#include <QDebug>
#include <QStyle>
#include <QFontDialog>
#include <QFileDialog>
#include <QScrollBar>
#include <QMessageBox>
#include <QKeySequence>
#include <QElapsedTimer>

#include <iostream>

#include <QFile>
#include <QTextStream>

static constexpr auto editTabIndex = 0;
static constexpr auto codeTabIndex = 1;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    ui_->setupUi(this);
    setupUI();
    setupActions();

    updateUI(viewModel_->uiState());

    connect(ui_->actionImport_Font, &QAction::triggered, this, &MainWindow::showFontDialog);
    connect(ui_->actionOpen, &QAction::triggered, this, &MainWindow::showOpenFaceDialog);
    connect(ui_->actionReset_Glyph, &QAction::triggered, this, &MainWindow::resetCurrentGlyph);
    connect(ui_->actionReset_Font, &QAction::triggered, this, &MainWindow::resetFont);

    connect(ui_->actionSave, &QAction::triggered, this, &MainWindow::saveOrSaveAs);

    connect(ui_->actionExport, &QAction::triggered, this, &MainWindow::exportSourceCode);
    connect(ui_->exportButton, &QPushButton::clicked, this, &MainWindow::exportSourceCode);

    connect(ui_->actionQuit, &QAction::triggered, &QApplication::quit);
    connect(ui_->tabWidget, &QTabWidget::currentChanged, [&](int index) {
        if (index == codeTabIndex) {
            viewModel_->prepareSourceCodeTab();
        }
    });
    connect(ui_->invertBitsCheckBox, &QCheckBox::stateChanged, [&](int state) {
        viewModel_->setInvertBits(state == Qt::Checked);
    });
    connect(ui_->bitNumberingCheckBox, &QCheckBox::stateChanged, [&](int state) {
        viewModel_->setMSBEnabled(state == Qt::Checked);
    });
    connect(ui_->lineSpacingCheckBox, &QCheckBox::stateChanged, [&](int state) {
        viewModel_->setIncludeLineSpacing(state == Qt::Checked);
    });
    connect(ui_->formatComboBox, &QComboBox::currentTextChanged,
            viewModel_.get(), &MainWindowModel::setOutputFormat);


    connect(viewModel_.get(), &MainWindowModel::documentTitleChanged, this, &MainWindow::updateDocumentTitle);
    connect(viewModel_.get(), &MainWindowModel::uiStateChanged, this, &MainWindow::updateUI);
    connect(viewModel_.get(), &MainWindowModel::faceLoaded, this, &MainWindow::displayFace);
    connect(viewModel_.get(), &MainWindowModel::activeGlyphChanged, this, &MainWindow::displayGlyph);
    connect(viewModel_.get(), &MainWindowModel::sourceCodeUpdating, [&]() {
//        ui_->stackedWidget->setCurrentWidget(ui_->spinnerContainer);
    });
    connect(viewModel_.get(), &MainWindowModel::sourceCodeChanged, [&](const QString& text) {
        ui_->stackedWidget->setCurrentWidget(ui_->sourceCodeContainer);
        ui_->sourceCodeTextBrowser->setPlainText(text);
    });
}

void MainWindow::setupUI()
{
    updateDocumentTitle(viewModel_->documentTitle());
    faceScene_->setBackgroundBrush(QBrush(Qt::lightGray));
    ui_->faceGraphicsView->setScene(faceScene_.get());

    ui_->faceInfoLabel->setVisible(false);

    auto scrollBarWidth = ui_->faceGraphicsView->verticalScrollBar()->sizeHint().width();
    auto faceViewWidth = static_cast<int>(FaceWidget::cell_width) * 3 + scrollBarWidth;
    ui_->faceGraphicsView->setMinimumSize({ faceViewWidth,
                                            ui_->faceGraphicsView->minimumSize().height() });

    ui_->invertBitsCheckBox->setCheckState(viewModel_->invertBits());
    ui_->bitNumberingCheckBox->setCheckState(viewModel_->msbEnabled());
    ui_->lineSpacingCheckBox->setCheckState(viewModel_->includeLineSpacing());

    for (const auto &pair : viewModel_->outputFormats().toStdMap()) {
        ui_->formatComboBox->addItem(pair.second, pair.first);
    }
    ui_->formatComboBox->setCurrentText(viewModel_->outputFormat());

    QFont f("Monaco", 13);
    f.setStyleHint(QFont::TypeWriter);
    ui_->sourceCodeTextBrowser->setFont(f);
}

void MainWindow::setupActions()
{
    auto undo = undoStack_->createUndoAction(this);
    undo->setIcon(QIcon {":/toolbar/assets/undo.svg"});
    undo->setShortcut(QKeySequence::Undo);

    auto redo = undoStack_->createRedoAction(this);
    redo->setIcon(QIcon {":/toolbar/assets/redo.svg"});
    redo->setShortcut(QKeySequence::Redo);

    ui_->openButton->setDefaultAction(ui_->actionOpen);
    ui_->importFontButton->setDefaultAction(ui_->actionImport_Font);
    ui_->addGlyphButton->setDefaultAction(ui_->actionAdd_Glyph);
    ui_->saveButton->setDefaultAction(ui_->actionSave);
    ui_->copyButton->setDefaultAction(ui_->actionCopy_Glyph);
    ui_->pasteButton->setDefaultAction(ui_->actionPaste_Glyph);
    ui_->undoButton->setDefaultAction(undo);
    ui_->redoButton->setDefaultAction(redo);
    ui_->resetGlyphButton->setDefaultAction(ui_->actionReset_Glyph);
    ui_->resetFontButton->setDefaultAction(ui_->actionReset_Font);
    ui_->actionReset_Glyph->setEnabled(false);
    ui_->actionReset_Font->setEnabled(false);

    ui_->menuEdit->insertAction(ui_->actionCopy_Glyph, undo);
    ui_->menuEdit->insertAction(ui_->actionCopy_Glyph, redo);
}

void MainWindow::updateUI(MainWindowModel::UIState uiState)
{
    ui_->tabWidget->setTabEnabled(1, uiState[MainWindowModel::InterfaceAction::ActionTabCode]);
    ui_->actionImport_Font->setEnabled(uiState[MainWindowModel::InterfaceAction::ActionImportFont]);
    ui_->actionOpen->setEnabled(uiState[MainWindowModel::InterfaceAction::ActionOpen]);
    ui_->actionAdd_Glyph->setEnabled(uiState[MainWindowModel::InterfaceAction::ActionAddGlyph]);
    ui_->actionSave->setEnabled(uiState[MainWindowModel::InterfaceAction::ActionSave]);
    ui_->actionSave_As->setEnabled(uiState[MainWindowModel::InterfaceAction::ActionSave]);
    ui_->actionCopy_Glyph->setEnabled(uiState[MainWindowModel::InterfaceAction::ActionCopy]);
    ui_->actionPaste_Glyph->setEnabled(uiState[MainWindowModel::InterfaceAction::ActionPaste]);
//    ui_->undoButton->defaultAction()->setEnabled(uiState[MainWindowModel::InterfaceAction::ActionUndo]);
//    ui_->redoButton->defaultAction()->setEnabled(uiState[MainWindowModel::InterfaceAction::ActionRedo]);
//    ui_->actionReset_Glyph->setEnabled(uiState[MainWindowModel::InterfaceAction::ActionResetGlyph]);
//    ui_->actionReset_Font->setEnabled(uiState[MainWindowModel::InterfaceAction::ActionResetFont]);
    ui_->actionExport->setEnabled(uiState[MainWindowModel::InterfaceAction::ActionExport]);
    ui_->actionPrint->setEnabled(uiState[MainWindowModel::InterfaceAction::ActionPrint]);
}

void MainWindow::showFontDialog()
{
    bool ok;
    QFont f("Monaco", 24);
    f.setStyleHint(QFont::TypeWriter);
    f = QFontDialog::getFont(&ok, f, this, tr("Select Font"), QFontDialog::MonospacedFonts | QFontDialog::DontUseNativeDialog);
    qDebug() << "selected font:" << f << "ok?" << ok;

//    viewModel_->registerInputEvent(MainWindowModel::ActionImportFont);
    viewModel_->importFont(f);
}

void MainWindow::showOpenFaceDialog()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open Font file"), QString(), tr("FontEdit documents (*.fontedit)"));
    if (!fileName.isNull()) {
        viewModel_->loadFace(fileName);
    }
}

void MainWindow::saveOrSaveAs()
{
    auto currentPath = viewModel_->currentDocumentPath();
    if (currentPath.has_value()) {
        viewModel_->saveFace(currentPath.value());
    } else {
        QString fileName = QFileDialog::getSaveFileName(this, "Save");
        viewModel_->saveFace(fileName);
    }
}

void MainWindow::displayFace(const Font::Face& face)
{
    if (faceWidget_ == nullptr) {
        faceWidget_ = new FaceWidget();
        ui_->faceGraphicsView->scene()->addItem(faceWidget_);

        connect(faceWidget_, &FaceWidget::currentGlyphIndexChanged,
                this, &MainWindow::switchActiveGlyph);
    }

    auto margins = viewModel_->faceModel()->originalFaceMargins();
    faceWidget_->load(face, margins);
    updateFaceInfoLabel(viewModel_->faceModel()->faceInfo());
    ui_->faceInfoLabel->setVisible(true);

    if (viewModel_->faceModel()->activeGlyphIndex().has_value()) {
        displayGlyph(viewModel_->faceModel()->activeGlyph());
    } else if (auto g = glyphWidget_.get()) {
        ui_->glyphGraphicsView->scene()->removeItem(g);
        glyphWidget_.release();
    }
}

void MainWindow::updateFaceInfoLabel(const FaceInfo &faceInfo)
{
    QStringList lines;
    lines << faceInfo.fontName;
    lines << tr("Size (full): %1x%2px").arg(faceInfo.size.width).arg(faceInfo.size.height);
    lines << tr("Size (adjusted): %1x%2px").arg(faceInfo.sizeWithoutMargins.width).arg(faceInfo.sizeWithoutMargins.height);
    lines << tr("%n Glyph(s)", "", faceInfo.numberOfGlyphs);
    ui_->faceInfoLabel->setText(lines.join("\n"));
}

void MainWindow::updateDocumentTitle(const QString &title)
{
    ui_->tabWidget->setTabText(editTabIndex, title);
}

void MainWindow::displayGlyph(const Font::Glyph& glyph)
{
    auto margins = viewModel_->faceModel()->originalFaceMargins();
    if (!glyphWidget_.get()) {
        glyphWidget_ = std::make_unique<GlyphWidget>(glyph, margins);
        ui_->glyphGraphicsView->scene()->addItem(glyphWidget_.get());

        connect(glyphWidget_.get(), &GlyphWidget::pixelsChanged,
                this, &MainWindow::editGlyph);
    } else {
        glyphWidget_->load(glyph, margins);
    }
    updateResetActions();
    ui_->glyphGraphicsView->fitInView(glyphWidget_->boundingRect(), Qt::KeepAspectRatio);
}

void MainWindow::editGlyph(const BatchPixelChange& change)
{
    auto currentIndex = viewModel_->faceModel()->activeGlyphIndex();
    if (currentIndex.has_value()) {

        auto applyChange = [&, currentIndex, change](BatchPixelChange::ChangeType type) -> std::function<void()> {
            return [&, currentIndex, change, type] {
                viewModel_->faceModel()->modifyGlyph(currentIndex.value(), change, type);
                updateResetActions();
                glyphWidget_->applyChange(change, type);
                faceWidget_->updateGlyphPreview(currentIndex.value(), viewModel_->faceModel()->activeGlyph());
                viewModel_->updateDocumentTitle();
            };
        };

        undoStack_->push(new Command(tr("Edit Glyph"),
                                     applyChange(BatchPixelChange::ChangeType::Reverse),
                                     applyChange(BatchPixelChange::ChangeType::Normal)));

        viewModel_->registerInputEvent(MainWindowModel::UserEditedGlyph);
    }
}

void MainWindow::switchActiveGlyph(std::size_t newIndex)
{
    auto currentIndex = viewModel_->faceModel()->activeGlyphIndex();
    if (currentIndex.has_value()) {
        auto idx = currentIndex.value();
        if (idx == newIndex) {
            return;
        }

        auto setGlyph = [&](std::size_t index) -> std::function<void()> {
            return [&, index] {
                faceWidget_->setCurrentGlyphIndex(index);
                viewModel_->setActiveGlyphIndex(index);
            };
        };

        undoStack_->push(new Command(tr("Switch Active Glyph"),
                                     setGlyph(idx),
                                     setGlyph(newIndex)));
    } else {
        viewModel_->setActiveGlyphIndex(newIndex);
    }
}

void MainWindow::resetCurrentGlyph()
{
    Font::Glyph currentGlyphState { viewModel_->faceModel()->activeGlyph() };
    auto glyphIndex = viewModel_->faceModel()->activeGlyphIndex().value();

    undoStack_->push(new Command(tr("Reset Glyph"), [&, currentGlyphState, glyphIndex] {
        viewModel_->faceModel()->modifyGlyph(glyphIndex, currentGlyphState);
        displayGlyph(viewModel_->faceModel()->activeGlyph());
        faceWidget_->updateGlyphPreview(glyphIndex, viewModel_->faceModel()->activeGlyph());
        viewModel_->updateDocumentTitle();
    }, [&, glyphIndex] {
        viewModel_->faceModel()->resetActiveGlyph();
        displayGlyph(viewModel_->faceModel()->activeGlyph());
        faceWidget_->updateGlyphPreview(glyphIndex, viewModel_->faceModel()->activeGlyph());
        viewModel_->updateDocumentTitle();
    }));
}

void MainWindow::resetFont()
{
    auto message = tr("Are you sure you want to reset all changes to the font? This operation cannot be undone.");
    auto result = QMessageBox::warning(this, tr("Reset Font"), message, QMessageBox::Reset, QMessageBox::Cancel);
    if (result == QMessageBox::Reset) {
        viewModel_->faceModel()->reset();
        undoStack_->clear();
        updateResetActions();
        displayFace(viewModel_->faceModel()->face());
    }
}

void MainWindow::updateResetActions()
{
    auto currentIndex = viewModel_->faceModel()->activeGlyphIndex();
    if (currentIndex.has_value()) {
        ui_->actionReset_Glyph->setEnabled(viewModel_->faceModel()->isGlyphModified(currentIndex.value()));
    } else {
        ui_->actionReset_Glyph->setEnabled(false);
    }
    ui_->actionReset_Font->setEnabled(viewModel_->faceModel()->isModified());
}

void MainWindow::exportSourceCode()
{
    auto fileName = QFileDialog::getSaveFileName(this, tr("Save file"));

    QFile output(fileName);
    if (output.open(QFile::WriteOnly | QFile::Truncate)) {
        output.write(ui_->sourceCodeTextBrowser->document()->toPlainText().toUtf8());
        output.close();
    }
}
