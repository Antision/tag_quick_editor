#include "settingsdialog.h"
#include "ui_settingsdialog.h"
#include "func.h"

#include <QColorDialog>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QPushButton>

#include <algorithm>

QVector<TagMarkOverride> SettingsDialog::overrides() const
{
    QVector<TagMarkOverride> result;
    result.reserve(m_items.size());
    for(const TagMarkOverride& item:m_items)
        if(!item.tag.trimmed().isEmpty()
            &&(!item.color.trimmed().isEmpty()||!item.icon.trimmed().isEmpty()))
            result.push_back(item);
    return result;
}

SettingsDialog::SettingsDialog(const QString& seedTag,QWidget* parent)
    :QDialog(parent),ui(new Ui::SettingsDialog)
{
    ui->setupUi(this);
    // The main window's sheet says `QWidget{color:white}`; a dialog is a child of
    // it, so without its own sheet this one would be white text on the light
    // system background. The dialog's own sheet is nearer, so it wins.
    setStyleSheet(QStringLiteral(
        "QDialog{background-color:rgb(27,27,27);}"
        "QLabel{color:white;background:transparent;}"
        "QGroupBox{color:white;border:1px solid #4c399e;border-radius:4px;"
        "margin-top:9px;padding-top:4px;}"
        "QGroupBox::title{subcontrol-origin:margin;left:8px;padding:0 4px;}"
        "QTableWidget{background-color:rgb(24,24,24);color:white;"
        "gridline-color:#3a3a3a;border:1px solid #4c399e;"
        "alternate-background-color:rgb(31,31,31);selection-background-color:#4c399e;"
        "selection-color:white;}"
        "QTableWidget::item{padding:2px 4px;}"
        "QHeaderView::section{background-color:rgb(40,40,40);color:white;"
        "border:none;border-right:1px solid #3a3a3a;padding:4px;}"
        "QLineEdit{background-color:rgb(35,35,35);color:white;"
        "border:1px solid #4c399e;border-radius:3px;padding:3px 6px;}"
        "QPushButton{color:white;border:1px solid #4c399e;background:transparent;"
        "padding:4px 14px;border-radius:3px;}"
        "QPushButton:hover{background:rgba(177,100,255,60);}"
        "QPushButton:disabled{color:#777;border-color:#555;}"));
    ui->table->horizontalHeader()->setSectionResizeMode(0,QHeaderView::Stretch);
    ui->table->horizontalHeader()->setSectionResizeMode(1,QHeaderView::ResizeToContents);
    ui->table->horizontalHeader()->setSectionResizeMode(2,QHeaderView::ResizeToContents);
    // Row numbers are noise here, and the vertical header has no sheet of its own:
    // it stayed a white strip with white digits on the dark dialog.
    ui->table->verticalHeader()->setVisible(false);

    m_items=tagDecorations;
    int seedRow=0;
    if(!seedTag.trimmed().isEmpty()){
        const QString seed=seedTag.trimmed();
        seedRow=-1;
        for(int row=0;row<int(m_items.size());++row){
            if(m_items[row].tag.compare(seed,Qt::CaseInsensitive)==0){
                seedRow=row;    // already has an override: edit that one
                break;
            }
        }
        if(seedRow<0){
            TagMarkOverride item;
            item.tag=seed;
            m_items.prepend(item);
            seedRow=0;
        }
    }
    reloadTable();
    if(!m_items.isEmpty()){
        // Open on the tag the user was looking at (or the first saved override
        // when the list was opened without one). selectRow() only changes the
        // selection, not the current cell, so the editor fields are filled
        // explicitly - a selection alone would leave m_row at -1 and the first
        // keystroke would go nowhere.
        seedRow=std::clamp(seedRow,0,int(m_items.size())-1);
        ui->table->setCurrentCell(seedRow,0);
        ui->table->selectRow(seedRow);
        fillEditor(seedRow);
    }

    connect(ui->table,&QTableWidget::currentCellChanged,this,
            [this](int row,int,int,int){ fillEditor(row); });
    connect(ui->addButton,&QPushButton::clicked,this,&SettingsDialog::addItem);
    connect(ui->removeButton,&QPushButton::clicked,this,&SettingsDialog::removeItem);
    connect(ui->colorButton,&QPushButton::clicked,this,&SettingsDialog::pickColor);
    connect(ui->iconButton,&QPushButton::clicked,this,&SettingsDialog::pickIcon);
    connect(ui->tagEdit,&QLineEdit::textChanged,this,[this]{ writeEditorToItem(); });
    connect(ui->colorEdit,&QLineEdit::textChanged,this,[this]{ writeEditorToItem(); });
    connect(ui->iconEdit,&QLineEdit::textChanged,this,[this]{ writeEditorToItem(); });
    // Save only: the caller writes config.json and repaints the lists.
    connect(ui->buttonBox,&QDialogButtonBox::accepted,this,&QDialog::accept);
    connect(ui->buttonBox,&QDialogButtonBox::rejected,this,&QDialog::reject);
}

SettingsDialog::~SettingsDialog()
{
    delete ui;
}

void SettingsDialog::reloadTable()
{
    m_filling=true;
    ui->table->setRowCount(int(m_items.size()));
    for(int row=0;row<int(m_items.size());++row)
        updateRow(row);
    m_filling=false;
}

void SettingsDialog::updateRow(int row)
{
    if(row<0||row>=int(m_items.size()))
        return;
    const TagMarkOverride& item=m_items[row];
    auto setCell=[this,row](int column,const QString& text){
        QTableWidgetItem* cell=ui->table->item(row,column);
        if(!cell){
            cell=new QTableWidgetItem;
            ui->table->setItem(row,column,cell);
        }
        cell->setText(text);
    };
    setCell(0,item.tag);
    setCell(1,item.color);
    setCell(2,item.icon);
}

void SettingsDialog::fillEditor(int row)
{
    m_row=row;
    const bool hasRow=row>=0&&row<int(m_items.size());
    for(QWidget* widget:{static_cast<QWidget*>(ui->tagEdit),static_cast<QWidget*>(ui->colorEdit),
                         static_cast<QWidget*>(ui->iconEdit),static_cast<QWidget*>(ui->colorButton),
                         static_cast<QWidget*>(ui->iconButton)})
        widget->setEnabled(hasRow);
    m_filling=true;
    if(hasRow){
        ui->tagEdit->setText(m_items[row].tag);
        ui->colorEdit->setText(m_items[row].color);
        ui->iconEdit->setText(m_items[row].icon);
    }else{
        ui->tagEdit->clear();
        ui->colorEdit->clear();
        ui->iconEdit->clear();
    }
    m_filling=false;
}

void SettingsDialog::writeEditorToItem()
{
    if(m_filling||m_row<0||m_row>=int(m_items.size()))
        return;
    TagMarkOverride& item=m_items[m_row];
    item.tag=ui->tagEdit->text().trimmed();
    item.color=ui->colorEdit->text().trimmed();
    item.icon=ui->iconEdit->text().trimmed();
    m_filling=true;
    updateRow(m_row);
    m_filling=false;
}

void SettingsDialog::addItem()
{
    TagMarkOverride item;
    // Starting from the text that is already in the field makes "add another
    // colour for this tag" a two-click job.
    item.tag=ui->tagEdit->text().trimmed();
    m_items.append(item);
    reloadTable();
    const int row=int(m_items.size())-1;
    ui->table->setCurrentCell(row,0);
    ui->table->selectRow(row);
    fillEditor(row);
    ui->tagEdit->setFocus();
    ui->tagEdit->selectAll();
}

void SettingsDialog::removeItem()
{
    if(m_row<0||m_row>=int(m_items.size()))
        return;
    m_items.removeAt(m_row);
    reloadTable();
    if(m_items.isEmpty()){
        fillEditor(-1);
        return;
    }
    const int row=std::min(m_row,int(m_items.size())-1);
    ui->table->setCurrentCell(row,0);
    ui->table->selectRow(row);
    fillEditor(row);
}

void SettingsDialog::pickColor()
{
    QColor initial(ui->colorEdit->text().trimmed());
    if(!initial.isValid())
        initial=Qt::white;
    const QColor color=QColorDialog::getColor(initial,this,QStringLiteral("Tag colour"));
    if(!color.isValid())
        return;
    ui->colorEdit->setText(color.name());
}

void SettingsDialog::pickIcon()
{
    const QString path=QFileDialog::getOpenFileName(
        this,QStringLiteral("Tag icon"),
        QCoreApplication::applicationDirPath()+QStringLiteral("/data/icons"),
        QStringLiteral("Images (*.png *.ico *.bmp *.jpg);;All files (*)"));
    if(path.isEmpty())
        return;
    // A file inside data/icons is stored by name: the config then survives moving
    // the whole installation.
    const QString iconDir=QDir(QCoreApplication::applicationDirPath()+QStringLiteral("/data/icons"))
                               .absolutePath();
    const QFileInfo info(path);
    const QString relative=QDir(iconDir).relativeFilePath(info.absoluteFilePath());
    ui->iconEdit->setText(relative.startsWith(QLatin1String(".."))
                              ?info.absoluteFilePath()
                              :relative);
}
