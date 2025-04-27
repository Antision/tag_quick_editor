
QString liststyle = QStringLiteral(R"(
teWordBase{
    font: %1pt "Segoe UI";
}
QListView{
    border:1px solid #4c399e;
}
QScrollArea{border:1px solid #4c399e;}
QScrollArea:hover{
    border-color: #6045d6;
}
QScrollArea:!hover{
    border-color: #4c399e;
}
QScrollBar{background:#5421ff;}
QScrollBar::vertical{margin: 12px 0px 12px 0px;border:0px solid #5421ff;width:10px;}
QScrollBar::horizontal{margin: 0px 12px 0px 12px;border:0px solid #5421ff;height:10px;}
QScrollBar::handle:!pressed{background:#7751f3;border:1px solid #c5a9f7;}
QScrollBar::handle:pressed{background:#c5a9f7;}

QScrollBar::handle:vertical{min-height:20px;}
QScrollBar::handle:vertical{min-width:20px;}
QScrollBar::up-arrow:vertical{
    border-image: url(":/res/scrollbar_uparrow.png");
    width:10px;height:10px;
}
QScrollBar::down-arrow:vertical{
    border-image: url(":/res/scrollbar_downarrow.png");
    width:10px;height:10px;
}

QScrollBar::left-arrow:horizontal {
    border-image: url(":/res/scrollbar_leftarrow.png");
    width:10px;height:10px;
}
QScrollBar::right-arrow:horizontal {
    border-image: url(":/res/scrollbar_rightarrow.png");
    width:10px;height:10px;
}
QScrollBar::sub-page,QScrollBar::add-page {
    background-color: #6d6292;
}
QScrollBar::sub-line:vertical {
    subcontrol-position: top;
    subcontrol-origin: margin;
    background-color: #6644d6;
    border: 0px solid #e7dfff;
    height:12px;
    width:10px;
}
QScrollBar::add-line:vertical {
    subcontrol-position: bottom;
    subcontrol-origin: margin;
    background-color: #6644d6;
    border: 0px solid #e7dfff;
    height: 12px;
    width:10px;
}

QScrollBar::sub-line:horizontal {
    subcontrol-position: left;
    subcontrol-origin: margin;
    background-color: #6644d6;
    border: 0px solid #e7dfff;
    height:10px;
    width:12px;
}
QScrollBar::add-line:horizontal {
    subcontrol-position: right;
    subcontrol-origin: margin;
    background-color: #6644d6;
    border: 0px solid #e7dfff;
    height: 10px;
    width:12px;
}
)");



