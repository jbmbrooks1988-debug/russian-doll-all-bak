
from PySide6.QtCore import (QCoreApplication, QDate, QDateTime, QLocale,
    QMetaObject, QObject, QPoint, QRect,
    QSize, QTime, QUrl, Qt)
from PySide6.QtGui import (QAction, QBrush, QColor, QConicalGradient,
    QCursor, QFont, QFontDatabase, QGradient,
    QIcon, QImage, QKeySequence, QLinearGradient,
    QPainter, QPalette, QPixmap, QRadialGradient,
    QTransform)
from PySide6.QtWidgets import (QApplication, QFrame, QGridLayout, QHeaderView,
    QLabel, QLineEdit, QMainWindow, QMenu,
    QMenuBar, QPushButton, QSizePolicy, QSpinBox,
    QStackedWidget, QStatusBar, QTabWidget, QTableWidget,
    QTableWidgetItem, QWidget,QFileDialog, QVBoxLayout, QHBoxLayout, QColorDialog)
import ressource, sys, csv, webbrowser

class Ui_MainWindow(QMainWindow):
    def setupUi(self, MainWindow):
        if not MainWindow.objectName():
            MainWindow.setObjectName(u"MainWindow")
        MainWindow.setWindowModality(Qt.ApplicationModal)
        MainWindow.resize(798, 791)
        sizePolicy = QSizePolicy(QSizePolicy.Preferred, QSizePolicy.Preferred)
        sizePolicy.setHorizontalStretch(0)
        sizePolicy.setVerticalStretch(0)
        sizePolicy.setHeightForWidth(MainWindow.sizePolicy().hasHeightForWidth())
        MainWindow.setSizePolicy(sizePolicy)

        self.actionOpen = QAction(MainWindow)
        self.actionOpen.setObjectName(u"actionOpen")
        icon = QIcon()
        icon.addFile(u":/icons/icons/file.svg", QSize(), QIcon.Normal, QIcon.Off)
        self.actionOpen.setIcon(icon)
        self.actionOpen.triggered.connect(self.load_csv)

        self.actionSave = QAction(MainWindow)
        self.actionSave.setObjectName(u"actionSave")
        icon1 = QIcon()
        icon1.addFile(u":/icons/icons/save.svg", QSize(), QIcon.Normal, QIcon.Off)
        self.actionSave.setIcon(icon1)
        self.actionSave.triggered.connect(self.save_file)

        self.actionSave_as = QAction(MainWindow)
        self.actionSave_as.setObjectName(u"actionSave_as")
        icon2 = QIcon()
        icon2.addFile(u":/icons/icons/folder.svg", QSize(), QIcon.Normal, QIcon.Off)
        self.actionSave_as.setIcon(icon2)
        self.actionSave_as.triggered.connect(self.saveas_file)

        self.actionNew = QAction(MainWindow)
        self.actionNew.setObjectName(u"actionNew")
        icon3 = QIcon()
        icon3.addFile(u":/icons/icons/plus-square.svg", QSize(), QIcon.Normal, QIcon.Off)
        self.actionNew.setIcon(icon3)
        self.actionNew.triggered.connect(self.add_table_page)
        self.actionCreate_table = QAction(MainWindow)
        self.actionCreate_table.setObjectName(u"actionCreate_table")
        self.actionCreate_table.setIcon(icon3)

        self.actionGithub_repo = QAction(MainWindow)
        self.actionGithub_repo.setObjectName(u"actionGithub_repo")
        icon4 = QIcon()
        icon4.addFile(u":/icons/icons/terminal.svg", QSize(), QIcon.Normal, QIcon.Off)
        self.actionGithub_repo.setIcon(icon4)
        self.actionGithub_repo.triggered.connect(lambda : self.web_ressources('https://github.com/Koo-Kie/CSVeditor'))

        self.actionHelp = QAction(MainWindow)
        self.actionHelp.setObjectName(u"actionHelp")
        icon5 = QIcon()
        icon5.addFile(u":/icons/icons/help-circle.svg", QSize(), QIcon.Normal, QIcon.Off)
        self.actionHelp.setIcon(icon5)
        self.actionHelp.triggered.connect(lambda : self.web_ressources('https://github.com/Koo-Kie/CSVeditor/issues'))

        self.centralwidget = QWidget(MainWindow)
        self.centralwidget.setObjectName(u"centralwidget")
        self.verticalLayout_4 = QVBoxLayout(self.centralwidget)
        self.verticalLayout_4.setSpacing(0)
        self.verticalLayout_4.setObjectName(u"verticalLayout_4")
        self.verticalLayout_4.setContentsMargins(0, 0, 0, 0)
        self.header = QFrame(self.centralwidget)
        self.header.setObjectName(u"header")
        self.header.setFrameShape(QFrame.StyledPanel)
        self.header.setFrameShadow(QFrame.Raised)
        self.gridLayout = QGridLayout(self.header)
        self.gridLayout.setObjectName(u"gridLayout")
        self.italic_button = QPushButton(self.header)
        self.italic_button.setObjectName(u"italic_button")
        font = QFont()
        font.setFamilies([u"NSimSun"])
        font.setPointSize(13)
        font.setItalic(True)
        self.italic_button.setFont(font)
        self.italic_button.setCursor(QCursor(Qt.PointingHandCursor))

        self.gridLayout.addWidget(self.italic_button, 0, 0, 1, 1)
        self.italic_button.clicked.connect(lambda : self.style_item('italic'))

        self.bold_button = QPushButton(self.header)
        self.bold_button.setObjectName(u"bold_button")
        font1 = QFont()
        font1.setPointSize(13)
        font1.setBold(True)
        self.bold_button.setFont(font1)
        self.bold_button.setCursor(QCursor(Qt.PointingHandCursor))

        self.gridLayout.addWidget(self.bold_button, 0, 1, 1, 1)
        self.bold_button.clicked.connect(lambda : self.style_item('bold'))

        self.normal_button = QPushButton(self.header)
        self.normal_button.setObjectName(u"normal_button")
        font2 = QFont()
        font2.setPointSize(13)
        self.normal_button.setFont(font2)
        self.normal_button.setCursor(QCursor(Qt.PointingHandCursor))

        self.gridLayout.addWidget(self.normal_button, 0, 2, 1, 1)
        self.normal_button.clicked.connect(lambda : self.style_item('normal'))

        self.font_size = QSpinBox(self.header)
        self.font_size.setObjectName(u"font_size")
        self.font_size.setFont(font2)
        self.font_size.setCursor(QCursor(Qt.SizeVerCursor))
        self.font_size.setValue(9)

        self.gridLayout.addWidget(self.font_size, 0, 3, 1, 1)
        self.font_size.valueChanged.connect(self.style_item)

        self.color_button = QPushButton(self.header)
        self.color_button.setObjectName(u"color_button")
        self.color_button.setCursor(QCursor(Qt.PointingHandCursor))
        icon6 = QIcon()
        icon6.addFile(u":/icons/icons/droplet.svg", QSize(), QIcon.Normal, QIcon.Off)
        self.color_button.setIcon(icon6)

        self.gridLayout.addWidget(self.color_button, 0, 4, 1, 1)
        self.color_button.clicked.connect(lambda : self.style_item('color'))


        self.verticalLayout_4.addWidget(self.header)

        self.stackedWidget = QStackedWidget(self.centralwidget)
        self.stackedWidget.setObjectName(u"stackedWidget")
        self.page = QWidget()
        self.page.setObjectName(u"page")
        self.horizontalLayout = QHBoxLayout(self.page)
        self.horizontalLayout.setSpacing(0)
        self.horizontalLayout.setObjectName(u"horizontalLayout")
        self.horizontalLayout.setContentsMargins(0, 0, 0, 0)

        self.table1 = QTableWidget(self.page)
        self.table1.setObjectName(u"table1")
        sizePolicy.setHeightForWidth(self.table1.sizePolicy().hasHeightForWidth())
        self.table1.setSizePolicy(sizePolicy)
        self.table1.setRowCount(100)
        self.table1.setColumnCount(100)

        self.horizontalLayout.addWidget(self.table1)

        self.stackedWidget.addWidget(self.page)

        self.verticalLayout_4.addWidget(self.stackedWidget)

        self.footer = QFrame(self.centralwidget)
        self.footer.setObjectName(u"footer")
        sizePolicy.setHeightForWidth(self.footer.sizePolicy().hasHeightForWidth())
        self.footer.setSizePolicy(sizePolicy)
        self.footer.setFrameShape(QFrame.StyledPanel)
        self.footer.setFrameShadow(QFrame.Raised)
        self.verticalLayout_3 = QVBoxLayout(self.footer)
        self.verticalLayout_3.setObjectName(u"verticalLayout_3")
        self.verticalLayout_3.setContentsMargins(-1, 0, -1, 9)
        self.tabWidget = QTabWidget(self.footer)
        self.tabWidget.setObjectName(u"tabWidget")
        sizePolicy.setHeightForWidth(self.tabWidget.sizePolicy().hasHeightForWidth())
        self.tabWidget.setSizePolicy(sizePolicy)
        self.tabWidget.setFont(font2)
        self.tabWidget.setCursor(QCursor(Qt.PointingHandCursor))
        self.tab_1 = QWidget()
        self.tab_1.setObjectName(u"tab_1")
        self.tabWidget.addTab(self.tab_1, "")

        self.verticalLayout_3.addWidget(self.tabWidget)

        self.search_fields = QFrame(self.footer)
        self.search_fields.setObjectName(u"search_fields")
        self.search_fields.setFrameShape(QFrame.StyledPanel)
        self.search_fields.setFrameShadow(QFrame.Raised)
        self.horizontalLayout_2 = QHBoxLayout(self.search_fields)
        self.horizontalLayout_2.setObjectName(u"horizontalLayout_2")
        self.search_label_2 = QLabel(self.search_fields)
        self.search_label_2.setObjectName(u"search_label_2")
        self.search_label_2.setFont(font2)

        self.horizontalLayout_2.addWidget(self.search_label_2)

        self.search_input = QLineEdit(self.search_fields)
        self.search_input.setObjectName(u"search_input")
        self.search_input.setFont(font2)
        self.search_input.setFrame(False)
        self.search_input.setClearButtonEnabled(False)

        self.horizontalLayout_2.addWidget(self.search_input)
        self.search_input.textChanged.connect(lambda : self.research('search'))

        self.occurence_label = QLabel(self.search_fields)
        self.occurence_label.setObjectName(u"occurence_label")
        self.occurence_label.setFont(font2)

        self.horizontalLayout_2.addWidget(self.occurence_label)

        self.replace_label = QLabel(self.search_fields)
        self.replace_label.setObjectName(u"replace_label")
        self.replace_label.setFont(font2)

        self.horizontalLayout_2.addWidget(self.replace_label)

        self.replace_input = QLineEdit(self.search_fields)
        self.replace_input.setObjectName(u"replace_input")
        self.replace_input.setFont(font2)
        self.replace_input.setFrame(False)

        self.horizontalLayout_2.addWidget(self.replace_input)

        self.replace_button = QPushButton(self.search_fields)
        self.replace_button.setObjectName(u"replace_button")
        font3 = QFont()
        font3.setPointSize(12)
        self.replace_button.setFont(font3)
        self.replace_button.setCursor(QCursor(Qt.PointingHandCursor))

        self.horizontalLayout_2.addWidget(self.replace_button)
        self.replace_button.clicked.connect(lambda : self.research('replace'))


        self.verticalLayout_3.addWidget(self.search_fields)


        self.verticalLayout_4.addWidget(self.footer)

        MainWindow.setCentralWidget(self.centralwidget)
        self.menubar = QMenuBar(MainWindow)
        self.menubar.setObjectName(u"menubar")
        self.menubar.setGeometry(QRect(0, 0, 798, 22))
        self.menuFile = QMenu(self.menubar)
        self.menuFile.setObjectName(u"menuFile")
        self.menuFile.setCursor(QCursor(Qt.PointingHandCursor))
        self.menuHelp = QMenu(self.menubar)
        self.menuHelp.setObjectName(u"menuHelp")
        sizePolicy.setHeightForWidth(self.menuHelp.sizePolicy().hasHeightForWidth())
        self.menuHelp.setSizePolicy(sizePolicy)
        self.menuHelp.setCursor(QCursor(Qt.PointingHandCursor))
        MainWindow.setMenuBar(self.menubar)
        self.statusbar = QStatusBar(MainWindow)
        self.statusbar.setObjectName(u"statusbar")
        MainWindow.setStatusBar(self.statusbar)

        self.menubar.addAction(self.menuFile.menuAction())
        self.menubar.addAction(self.menuHelp.menuAction())
        self.menuFile.addAction(self.actionNew)
        self.menuFile.addAction(self.actionOpen)
        self.menuFile.addSeparator()
        self.menuFile.addAction(self.actionSave)
        self.menuFile.addAction(self.actionSave_as)
        self.menuHelp.addAction(self.actionGithub_repo)
        self.menuHelp.addAction(self.actionHelp)

        self.retranslateUi(MainWindow)

        self.tabWidget.tabBarClicked.connect(self.stackedWidget.setCurrentIndex)

        self.stackedWidget.setCurrentIndex(0)
        self.tabWidget.setCurrentIndex(0)


        QMetaObject.connectSlotsByName(MainWindow)

        self.tab_counter = 1
        self.filepath = {}
    

    def add_table_page(self):
        self.tab_counter += 1
        # Create a new tab in the tab widget
        new_tab_index = self.tabWidget.addTab(QWidget(),f"Table {self.tab_counter}")
        
        # Create a new page in the stacked widget
        new_page_widget = QWidget()
        new_page_layout = QVBoxLayout(new_page_widget)
        new_page_layout.setContentsMargins(0, 0, 0, 0)
        new_table_widget = QTableWidget()
        new_page_layout.addWidget(new_table_widget)
        self.stackedWidget.addWidget(new_page_widget)
        
        # Associate the new tab with the new page
        self.tabWidget.setTabToolTip(new_tab_index, f"Table {self.tab_counter}")
        self.tabWidget.setTabWhatsThis(new_tab_index, f"Table {self.tab_counter}")
        self.tabWidget.setTabText(new_tab_index, f"Table {self.tab_counter}")
        self.tabWidget.setCurrentIndex(new_tab_index)
        self.stackedWidget.setCurrentIndex(new_tab_index)

        new_table_widget.setRowCount(100)
        new_table_widget.setColumnCount(100)

        # cols = [chr(i) for i in range(ord('A'), ord('Z')+1)]

        # # Add second set of column labels (AA to AZ)
        # for i in range(ord('A'), ord('Z')+1):
        #     cols.append('A' + chr(i))

        # # Set row labels (numbers 1 to 100)
        # rows = [str(i+1) for i in range(100)]

        # # Set up table with row and column labels
        # new_table_widget.setColumnCount(len(cols))
        # new_table_widget.setRowCount(len(rows))
        # new_table_widget.setHorizontalHeaderLabels(cols)
        # new_table_widget.setVerticalHeaderLabels(rows)

        # # Set each cell to an empty QTableWidgetItem
        # for i in range(len(rows)):
        #     for j in range(len(cols)):
        #         new_table_widget.setItem(i, j, QTableWidgetItem(""))

    def load_csv(self):
        # Open a file dialog to select a CSV file
        index = self.stackedWidget.currentIndex()
        self.filepath[index], _ = QFileDialog.getOpenFileName(self, "Open CSV", "", "CSV files (*.csv)")
        if not self.filepath:
            return

        # Get the current QTableWidget on the current page
        current_page_index = self.stackedWidget.currentIndex()
        current_page_widget = self.stackedWidget.widget(current_page_index)
        current_table_widget = current_page_widget.findChild(QTableWidget)

        # Clear the current table
        current_table_widget.clear()

        # Get the number of rows and columns in the CSV file
        with open(self.filepath[index], "r") as f:
            reader = csv.reader(f)
            num_rows = len(list(reader))
            f.seek(0)
            num_cols = len(next(reader))

        # Set the row and column count of the table widget
        current_table_widget.setRowCount(num_rows + 100)
        current_table_widget.setColumnCount(num_cols + 100)

        # Load the CSV data into the table
        with open(self.filepath[index], "r") as f:
            reader = csv.reader(f)
            for row_index, row_data in enumerate(reader):
                for column_index, cell_data in enumerate(row_data):
                    item = QTableWidgetItem(cell_data)
                    current_table_widget.setItem(row_index, column_index, item)

        # Change the current tab to the page containing the table
        self.tabWidget.setCurrentIndex(current_page_index)




    def save_file(self):
        # Get the current QTableWidget on the current page
        current_page_widget = self.stackedWidget.widget(self.stackedWidget.currentIndex())
        current_table_widget = current_page_widget.findChild(QTableWidget)

        # Get the current file path (if any)
        current_file_path = self.filepath.get(self.stackedWidget.currentIndex())

        # If no file path exists, prompt the user for a file path
        if not current_file_path:
            current_file_path, _ = QFileDialog.getSaveFileName(self, "Save CSV", "", "CSV files (*.csv)")

        # If the user cancels the save dialog, return
        if not current_file_path:
            return

        # Open the file and write the table data to it
        with open(current_file_path, "w", newline="") as f:
            writer = csv.writer(f)
            for row_index in range(current_table_widget.rowCount()):
                row_data = []
                for column_index in range(current_table_widget.columnCount()):
                    item = current_table_widget.item(row_index, column_index)
                    cell_value = item.text() if item is not None else ""
                    row_data.append(cell_value)
                writer.writerow(row_data)
            
        with open(current_file_path, "r") as f:
            reader = csv.reader(f)
            data = [row for row in reader]

        # Strip any extra commas from the end of each row
        for row in data:
            while len(row) > 0 and row[-1] == "":
                row.pop()

        # Open the output file for writing
        with open(current_file_path, "w", newline="") as f:
            writer = csv.writer(f)
            writer.writerows(data)

        # Store the file path for future reference
        self.filepath[self.stackedWidget.currentIndex()] = current_file_path


    
    def saveas_file(self):
        # Get the current QTableWidget on the current page
        current_page_widget = self.stackedWidget.widget(self.stackedWidget.currentIndex())
        current_table_widget = current_page_widget.findChild(QTableWidget)

        # Get the current file path (if any)
        current_file_path = self.filepath.get(self.stackedWidget.currentIndex())

        current_file_path, _ = QFileDialog.getSaveFileName(self, "Save CSV", "", "CSV files (*.csv)")

        # If the user cancels the save dialog, return
        if not current_file_path:
            return

        # Open the file and write the table data to it
        with open(current_file_path, "w", newline="") as f:
            writer = csv.writer(f)
            for row_index in range(current_table_widget.rowCount()):
                row_data = []
                for column_index in range(current_table_widget.columnCount()):
                    item = current_table_widget.item(row_index, column_index)
                    cell_value = item.text() if item is not None else ""
                    row_data.append(cell_value)
                writer.writerow(row_data)
            
        with open(current_file_path, "r") as f:
            reader = csv.reader(f)
            data = [row for row in reader]

        # Strip any extra commas from the end of each row
        for row in data:
            while len(row) > 0 and row[-1] == "":
                row.pop()

        # Open the output file for writing
        with open(current_file_path, "w", newline="") as f:
            writer = csv.writer(f)
            writer.writerows(data)

        # Store the file path for future reference
        self.filepath[self.stackedWidget.currentIndex()] = current_file_path
    
    def web_ressources(self, ressource):
        webbrowser.open(ressource)

    def style_item(*args):
        try: 
            self = args[0]
            current_page_widget = self.stackedWidget.widget(self.stackedWidget.currentIndex())
            current_table_widget = current_page_widget.findChild(QTableWidget)
            if args[1] == 'italic':
                for item in current_table_widget.selectedItems():
                    font = item.font()
                    font.setItalic(True)
                    item.setFont(font)
            elif args[1] == 'bold':
                for item in current_table_widget.selectedItems():
                    font = item.font()
                    font.setBold(True)
                    item.setFont(font)
            elif args[1] == 'normal':
                for item in current_table_widget.selectedItems():
                    font = item.font()
                    font.setBold(False)
                    font.setItalic(False)
                    item.setFont(font)
            elif args[1] == 'color':
                dialog = QColorDialog()
                if dialog.exec() == QColorDialog.Accepted:
                    selected_color = dialog.currentColor()
                    for item in current_table_widget.selectedItems():
                        item.setForeground(QColor(selected_color.name()))
            elif type(args[1]) == int:
                for item in current_table_widget.selectedItems():
                    font = item.font()
                    font.setPointSize(args[1])
                    item.setFont(font)       
        except:
            pass

    def research(*args):
        self = args[0]
        current_page_widget = self.stackedWidget.widget(self.stackedWidget.currentIndex())
        current_table_widget = current_page_widget.findChild(QTableWidget)
        if args[1] == 'search':
            query = self.search_input.text()
            occurences = 0
            if query:
                for row in range(current_table_widget.rowCount()):
                    for column in range(current_table_widget.columnCount()):
                        cell = current_table_widget.item(row, column)
                        if cell is not None:
                            if cell.text() == query:
                                occurences += 1
                                cell.setBackground(QColor("yellow"))
                            else:
                                cell.setBackground(QBrush())
            else:
                for row in range(current_table_widget.rowCount()):
                    for column in range(current_table_widget.columnCount()):
                        cell = current_table_widget.item(row, column)
                        if cell is not None:
                            cell.setBackground(QBrush())

            self.occurence_label.setText(str(occurences))
        elif args[1] == 'replace':
            query = self.search_input.text()
            new_value = self.replace_input.text()
            for row in range(current_table_widget.rowCount()):
                for column in range(current_table_widget.columnCount()):
                    cell = current_table_widget.item(row, column)
                    if cell is not None:
                        if cell.text() == query:
                            cell.setText(new_value)
                            cell.setBackground(QBrush())


    def retranslateUi(self, MainWindow):
        MainWindow.setWindowTitle(QCoreApplication.translate("MainWindow", u"CSVeditor by Kais and Adam", None))
        self.actionOpen.setText(QCoreApplication.translate("MainWindow", u"Open", None))
#if QT_CONFIG(shortcut)
        self.actionOpen.setShortcut(QCoreApplication.translate("MainWindow", u"Ctrl+O", None))
#endif // QT_CONFIG(shortcut)
        self.actionSave.setText(QCoreApplication.translate("MainWindow", u"Save", None))
#if QT_CONFIG(shortcut)
        self.actionSave.setShortcut(QCoreApplication.translate("MainWindow", u"Ctrl+S", None))
#endif // QT_CONFIG(shortcut)
        self.actionSave_as.setText(QCoreApplication.translate("MainWindow", u"Save as", None))
        self.actionNew.setText(QCoreApplication.translate("MainWindow", u"New", None))
#if QT_CONFIG(shortcut)
        self.actionNew.setShortcut(QCoreApplication.translate("MainWindow", u"Ctrl+N", None))
#endif // QT_CONFIG(shortcut)
        self.actionCreate_table.setText(QCoreApplication.translate("MainWindow", u"Create table", None))
#if QT_CONFIG(shortcut)
        self.actionCreate_table.setShortcut(QCoreApplication.translate("MainWindow", u"Ctrl+T", None))
#endif // QT_CONFIG(shortcut)
        self.actionGithub_repo.setText(QCoreApplication.translate("MainWindow", u"Github repo", None))
        self.actionHelp.setText(QCoreApplication.translate("MainWindow", u"Help", None))
        self.italic_button.setText(QCoreApplication.translate("MainWindow", u"I", None))
        self.bold_button.setText(QCoreApplication.translate("MainWindow", u"B", None))
        self.normal_button.setText(QCoreApplication.translate("MainWindow", u"A", None))
        self.color_button.setText("")
        self.tabWidget.setTabText(self.tabWidget.indexOf(self.tab_1), QCoreApplication.translate("MainWindow", u"Table 1", None))
        self.search_label_2.setText(QCoreApplication.translate("MainWindow", u"Search:", None))
        self.occurence_label.setText(QCoreApplication.translate("MainWindow", u"0", None))
        self.replace_label.setText(QCoreApplication.translate("MainWindow", u"Replace by:", None))
        self.replace_input.setText("")
        self.replace_button.setText(QCoreApplication.translate("MainWindow", u"Replace all", None))
        self.menuFile.setTitle(QCoreApplication.translate("MainWindow", u"File", None))
        self.menuHelp.setTitle(QCoreApplication.translate("MainWindow", u"Help", None))
    # retranslateUi

if __name__ == "__main__":
    app = QApplication(sys.argv)
    MainWindow = QMainWindow()
    ui = Ui_MainWindow()
    ui.setupUi(MainWindow)
    MainWindow.show()
    sys.exit(app.exec())