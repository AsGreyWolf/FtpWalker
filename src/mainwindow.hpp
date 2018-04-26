#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "FtpWalker.hpp"
#include <QtWidgets/QLabel>
#include <QtWidgets/QMainWindow>
#include <memory>
#include <unordered_map>

namespace Ui {
class MainWindow;
}

class FtpWalker;

class MainWindow : public QMainWindow {
	Q_OBJECT

	std::unordered_map<std::string, size_t> extensionSizes;

public:
	explicit MainWindow(QWidget *parent = 0);
	~MainWindow() override;

private:
	bool startEnabled;
	std::unique_ptr<Ui::MainWindow> ui;
	std::unique_ptr<FtpWalker> walker;
	std::unique_ptr<QLabel> status;

signals:
	void startGrabbing(const FtpWalker::HostInfo &host,
	                   const FtpWalker::AuthInfo &auth);
	void stopGrabbing();
};

#endif // MAINWINDOW_H
