#include "app.hpp"
#include "rootmenu.hpp"
#include "playengine.hpp"
#include "mainwindow.hpp"
#include "mrl.hpp"
#include "record.hpp"

#if defined(Q_OS_MAC)
#include "app_mac.hpp"
#elif defined(Q_OS_LINUX)
#include "app_x11.hpp"
#endif

#define APP_GROUP _L("application")

struct App::Data {
	QStringList styleNames;
#ifdef Q_OS_MAC
	QMenuBar *mb = new QMenuBar;
#else
	QMenuBar *mb = nullptr;
#endif
	MainWindow *main = nullptr;
#if defined(Q_OS_MAC)
	AppMac helper;
#elif defined(Q_OS_LINUX)
	AppX11 helper;
#endif
};

App::App(int &argc, char **argv)
: QtSingleApplication("net.xylosper.CMPlayer", argc, argv), d(new Data) {
	setOrganizationName("xylosper");
	setOrganizationDomain("xylosper.net");
	setApplicationName("CMPlayer");
	setQuitOnLastWindowClosed(false);
	setApplicationDisplayName("CMPlayer");
//	setFont(QFont(QString::fromUtf8("나눔 고딕")));
#ifndef Q_OS_MAC
	setWindowIcon(defaultIcon());
#endif
	auto makeStyleNameList = [this] () {
		auto names = QStyleFactory::keys();
		const auto defaultName = style()->objectName();
		for (auto it = ++names.begin(); it != names.end(); ++it) {
			if (defaultName.compare(*it, Qt::CaseInsensitive) == 0) {
				const auto name = *it;
				names.erase(it);
				names.prepend(name);
				break;
			}
		}
		return names;
	};

	auto makeStyle = [this]() {
		Record r(APP_GROUP);
		auto name = r.value("style", styleName()).toString();
		if (style()->objectName().compare(name, Qt::CaseInsensitive) == 0)
			return;
		if (!d->styleNames.contains(name, Qt::CaseInsensitive))
			return;
		setStyle(QStyleFactory::create(name));
	};

	d->styleNames = makeStyleNameList();
	makeStyle();
	connect(this, SIGNAL(messageReceived(QString)), this, SLOT(onMessageReceived(QString)));
}

App::~App() {
	delete d->mb;
	delete d;
}

void App::setMainWindow(MainWindow *mw) {
	d->main = mw;
#ifndef Q_OS_MAC
	d->main->setIcon(defaultIcon());
#endif
	setActivationWindow(d->main, false);
}

void App::setFileName(const QString &fileName) {
	if (d->main) {
		d->main->setTitle(fileName);
#ifdef Q_OS_LINUX
		d->helper.setWmName(fileName % _L(" - ") % applicationDisplayName());
#endif
	}
}

MainWindow *App::mainWindow() const {
	return d->main;
}

QIcon App::defaultIcon() {
	static QIcon icon;
	static bool init = false;
	if (!init) {
		icon.addFile(":/img/cmplayer16.png", QSize(16, 16));
		icon.addFile(":/img/cmplayer22.png", QSize(22, 22));
		icon.addFile(":/img/cmplayer24.png", QSize(24, 24));
		icon.addFile(":/img/cmplayer32.png", QSize(32, 32));
		icon.addFile(":/img/cmplayer48.png", QSize(48, 48));
		icon.addFile(":/img/cmplayer64.png", QSize(64, 64));
		icon.addFile(":/img/cmplayer128.png", QSize(128, 128));
		icon.addFile(":/img/cmplayer256.png", QSize(256, 256));
		icon.addFile(":/img/cmplayer-logo.png", QSize(512, 512));
		init = true;
	}
	return icon;
}

void App::setAlwaysOnTop(QWindow *window, bool onTop) {
	d->helper.setAlwaysOnTop(window, onTop);
}

void App::setScreensaverDisabled(bool disabled) {
	d->helper.setScreensaverDisabled(disabled);
}

bool App::event(QEvent *event) {
	switch ((int)event->type()) {
	case QEvent::FileOpen: {
		if (d->main)
			d->main->openFromFileManager(Mrl(static_cast<QFileOpenEvent*>(event)->url().toString()));
		event->accept();
		return true;
	} case ReopenEvent:
		d->main->show();
		event->accept();
		return true;
	default:
		return QApplication::event(event);
	}
}

QWindow *App::topWindow() const {
	return d->main;
}

QStringList App::devices() const {
	return d->helper.devices();
}

#ifdef Q_OS_MAC
QMenuBar *App::globalMenuBar() const {
	return d->mb;
}
#endif

Mrl App::getMrlFromCommandLine() {
	const auto args = parse(arguments());
	for (const Argument &arg : args) {
		qDebug() << arg.name << arg.value;
		if (arg.name == _L("open"))
			return Mrl(arg.value);
	}
	return Mrl();
}

Arguments App::parse(const QStringList &cmds) {
	Arguments args;
	for (const QString &cmd : cmds) {
		if (!cmd.startsWith(_L("--")))
			continue;
		Argument arg;
		const int eq = cmd.indexOf('=');
		if (eq < 0)
			arg.name = cmd.mid(2);
		else {
			arg.name = cmd.mid(2, eq - 2);
			arg.value = cmd.mid(eq+1);
		}
		args.append(arg);
	}
	return args;
}

void App::open(const QString &mrl) {
	if (!mrl.isEmpty() && d->main)
		d->main->openMrl(mrl);
}

void App::onMessageReceived(const QString &message) {
	const auto args = parse(message.split("[:sep:]"));
	for (const Argument &arg : args) {
		if (arg.name == _L("wake-up"))
			activateWindow();
		else if (arg.name == _L("open")) {
			const Mrl mrl(arg.value);
			if (!mrl.isEmpty() && d->main)
				d->main->openMrl(mrl);
		} else if (arg.name == _L("action")) {
			if (d->main)
				RootMenu::execute(arg.value);
		}
	}
}

QStringList App::availableStyleNames() const {
	return d->styleNames;
}

void App::setStyleName(const QString &name) {
	if (!d->styleNames.contains(name, Qt::CaseInsensitive))
		return;
	if (style()->objectName().compare(name, Qt::CaseInsensitive) == 0)
		return;
	setStyle(QStyleFactory::create(name));
	Record r(APP_GROUP);
	r.write(name, "style");
}

void App::setUnique(bool unique) {
	Record r(APP_GROUP);
	r.write(unique, "unique");
}

QString App::styleName() const {
	return style()->objectName();
}

bool App::isUnique() const {
	Record r(APP_GROUP);
	return r.value("unique", true).toBool();
}

#undef APP_GROUP

bool App::shutdown() {
	return d->helper.shutdown();
}
