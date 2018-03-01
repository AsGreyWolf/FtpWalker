#include "app.hpp"
#include "mainwindow.hpp"

std::unique_ptr<QApplication> app;

int main(int argc, char *argv[]) {
	app = std::make_unique<QApplication>(argc, argv);
	MainWindow w;
	w.show();
	return app->exec();
}
