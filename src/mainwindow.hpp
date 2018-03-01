#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QtWidgets/QLabel>
#include <QtWidgets/QMainWindow>
#include <memory>

namespace Ui {
class MainWindow;
}
class HttpWalker;

class MainWindow : public QMainWindow {
	Q_OBJECT

public:
	explicit MainWindow(QWidget *parent = 0);
	~MainWindow() override;

private:
	bool startEnabled;
	std::unique_ptr<Ui::MainWindow> ui;
	std::unique_ptr<HttpWalker> walker;
	std::unique_ptr<QLabel> status;

signals:
	void startGrabbing(const std::string &url);
	void stopGrabbing();
};

#endif // MAINWINDOW_H
