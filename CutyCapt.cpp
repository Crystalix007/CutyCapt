////////////////////////////////////////////////////////////////////
//
// CutyCapt - A Qt WebKit Web Page Rendering Capture Utility
//
// Copyright (C) 2003-2013 Bjoern Hoehrmann <bjoern@hoehrmann.de>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// $Id$
//
////////////////////////////////////////////////////////////////////

#include <QApplication>
#include <QSvgGenerator>
#include <QtGui>
#include <QtWebEngine>

#include <QPrinter>

#include "CutyCapt.hpp"
#include <QByteArray>
#include <QNetworkProxy>
#include <QNetworkRequest>
#include <QTimer>
#include <cstdlib>
#include <iostream>
#include <qsgrendererinterface.h>
#include <qwebenginesettings.h>

#if 0
#	define CUTYCAPT_SCRIPT 1
#endif

#ifdef STATIC_PLUGINS
Q_IMPORT_PLUGIN(qjpeg)
Q_IMPORT_PLUGIN(qgif)
Q_IMPORT_PLUGIN(qtiff)
Q_IMPORT_PLUGIN(qsvg)
Q_IMPORT_PLUGIN(qmng)
Q_IMPORT_PLUGIN(qico)
#endif

static struct _CutyExtMap {
	CutyCapt::OutputFormat id;
	const char* extension;
	const char* identifier;
} const CutyExtMap[] = {
	{ CutyCapt::SvgFormat, ".svg", "svg" },    { CutyCapt::PdfFormat, ".pdf", "pdf" },
	{ CutyCapt::PsFormat, ".ps", "ps" },       { CutyCapt::InnerTextFormat, ".txt", "itext" },
	{ CutyCapt::HtmlFormat, ".html", "html" }, { CutyCapt::JpegFormat, ".jpeg", "jpeg" },
	{ CutyCapt::PngFormat, ".png", "png" },    { CutyCapt::MngFormat, ".mng", "mng" },
	{ CutyCapt::TiffFormat, ".tiff", "tiff" }, { CutyCapt::GifFormat, ".gif", "gif" },
	{ CutyCapt::BmpFormat, ".bmp", "bmp" },    { CutyCapt::PpmFormat, ".ppm", "ppm" },
	{ CutyCapt::XbmFormat, ".xbm", "xbm" },    { CutyCapt::XpmFormat, ".xpm", "xpm" },
	{ CutyCapt::OtherFormat, "", "" }
};

QString CutyPage::chooseFile(QWebEnginePage* /*frame*/, const QString& /*suggestedFile*/) {
	return QString{};
}

bool CutyPage::javaScriptConfirm(QWebEnginePage* /*frame*/, const QString& /*msg*/) {
	return true;
}

bool CutyPage::javaScriptPrompt(QWebEnginePage* /*frame*/, const QString& /*msg*/,
                                const QString& /*defaultValue*/, QString* /*result*/) {
	return true;
}

void CutyPage::javaScriptConsoleMessage(const QString& /*message*/, int /*lineNumber*/,
                                        const QString& /*sourceID*/) {
	// noop
}

void CutyPage::javaScriptAlert(QWebEnginePage* /*frame*/, const QString& msg) {
	if (mPrintAlerts)
		qDebug() << "[alert]" << msg;

	if (mAlertString == msg) {
		QTimer::singleShot(10, mCutyCapt, SLOT(Delayed()));
	}
}

QString CutyPage::userAgentForUrl(/* const QUrl& url */) const {
	if (!mUserAgent.isNull())
		return mUserAgent;

	return page()->profile()->httpUserAgent();
}

void CutyPage::setUserAgent(const QString& userAgent) {
	mUserAgent = userAgent;
}

void CutyPage::setAlertString(const QString& alertString) {
	mAlertString = alertString;
}

QString CutyPage::getAlertString() {
	return mAlertString;
}

void CutyPage::setCutyCapt(CutyCapt* cutyCapt) {
	mCutyCapt = cutyCapt;
}

void CutyPage::setPrintAlerts(bool printAlerts) {
	mPrintAlerts = printAlerts;
}

void CutyPage::setAttribute(QWebEngineSettings::WebAttribute option, const QString& value) {
	if (value == "on")
		settings()->setAttribute(option, true);
	else if (value == "off")
		settings()->setAttribute(option, false);
	else
		(void)0; // TODO: ...
}

void CutyPage::setAttribute(Qt::WidgetAttribute option, const bool value) {
	QWidget::setAttribute(option, value);
}

// TODO: Consider merging some of main() and CutyCap

CutyCapt::CutyCapt(CutyPage* page, const QString& output, int delay, OutputFormat format,
                   const QString& scriptProp, const QString& scriptCode, bool insecure, bool smooth,
                   bool silent) {
	mPage = page;
	mOutput = output;
	mDelay = delay;
	mInsecure = insecure;
	mSmooth = smooth;
	mSilent = silent;
	mSawInitialLayout = false;
	mSawDocumentComplete = false;
	mSawGeometryChange = false;
	mFormat = format;
	mScriptProp = scriptProp;
	mScriptCode = scriptCode;
	mScriptObj = new QObject();

	// This is not really nice, but some restructuring work is
	// needed anyway, so this should not be that bad for now.
	mPage->setCutyCapt(this);
}

void CutyCapt::InitialLayoutCompleted() {
	if (!mSilent)
		std::cerr << "WebEngine completed initial layout" << std::endl;

	mSawInitialLayout = true;

	if (mSawInitialLayout && mSawDocumentComplete && mSawGeometryChange)
		TryDelayedRender();
}

void CutyCapt::DocumentComplete(bool /*ok*/) {
	if (!mSilent)
		std::cerr << "WebEngine completely downloaded document" << std::endl;

	mSawDocumentComplete = true;

	if (/*mSawInitialLayout && */ mSawDocumentComplete && mSawGeometryChange)
		TryDelayedRender();
}

void CutyCapt::JavaScriptWindowObjectCleared() {
	if (!mScriptProp.isEmpty()) {
		mPage->page()->runJavaScript(mScriptProp, [this](const QVariant& result) {
			QObject* obj = result.value<QObject*>();

			if (obj == mScriptObj)
				return;

			// mPage->addToJavaScriptWindowObject(mScriptProp, mScriptObj);
			mPage->page()->runJavaScript(mScriptCode);
		});
	}
}

void CutyCapt::TryDelayedRender() {
	if (!mPage->getAlertString().isEmpty())
		return;

	if (mDelay > 0) {
		QTimer::singleShot(mDelay, this, SLOT(Delayed()));
		return;
	}

	saveSnapshot();
	QApplication::exit();
}

void CutyCapt::Timeout() {
	saveSnapshot();
	QApplication::exit();
}

void CutyCapt::Delayed() {
	saveSnapshot();
	QApplication::exit();
}

void CutyCapt::onSizeChanged(const QSizeF& size) {
	std::cerr << "Geometry of viewport change (" << size.width() << ", " << size.height() << ")"
	          << std::endl;

	mViewSize = size.toSize();
	mPage->setMinimumSize(mViewSize);
	mSawGeometryChange = true;
}

void CutyCapt::saveSnapshot() {
	QPainter painter;
	const char* format = NULL;

	for (int ix = 0; CutyExtMap[ix].id != OtherFormat; ++ix)
		if (CutyExtMap[ix].id == mFormat)
			format = CutyExtMap[ix].identifier; //, break;

	// TODO: sometimes contents/viewport can have size 0x0
	// in which case saving them will fail. This is likely
	// the result of the method being called too early. So
	// far I've been unable to find a workaround, except
	// using --delay with some substantial wait time. I've
	// tried to resize multiple time, make a fake render,
	// check for other events... This is primarily a problem
	// under my Ubuntu virtual machine.

	// mPage->view()->setMinimumSize( mainFrame->contentsSize() );
	// uses the viewSize set by geometryChangeRequestedSlot

	QString mOutput{ this->mOutput };

	switch (mFormat) {
		case SvgFormat: {
			QSvgGenerator svg;
			svg.setFileName(mOutput);
			svg.setSize(mViewSize);
			painter.begin(&svg);
			mPage->render(&painter);
			painter.end();
			break;
		}
		case PdfFormat:
		case PsFormat: {
			QPrinter printer;
			printer.setPageSize(QPrinter::A4);
			printer.setOutputFileName(mOutput);
			// TODO: change quality here?
			mPage->page()->print(&printer, [](bool) {});
			break;
		}
		case InnerTextFormat:
			mPage->page()->toPlainText([mOutput](const QString& result) {
				QFile file(mOutput);
				file.open(QIODevice::WriteOnly | QIODevice::Text);
				QTextStream s(&file);
				s.setCodec("utf-8");
				s << result;
			});
			break;
		case HtmlFormat: {
			mPage->page()->toHtml([mOutput](const QString& result) {
				QFile file(mOutput);
				file.open(QIODevice::WriteOnly | QIODevice::Text);
				QTextStream s(&file);
				s.setCodec("utf-8");
				s << result;
			});
			break;
		}
		default: {
			// mPage->grab().save(mOutput, format);
			QImage image(mViewSize, QImage::Format_ARGB32);
			painter.begin(&image);
			if (mSmooth) {
				painter.setRenderHint(QPainter::SmoothPixmapTransform);
				painter.setRenderHint(QPainter::Antialiasing);
				painter.setRenderHint(QPainter::TextAntialiasing);
				painter.setRenderHint(QPainter::HighQualityAntialiasing);
			}
			mPage->render(&painter);
			painter.end();
			// TODO: add quality
			image.save(mOutput, format);
		}
	};
}

void CaptHelp(void) {
	printf("%s",
	       " ----------------------------------------------------------------------------------\n"
	       " Usage: CutyCapt --url=http://www.example.org/ --out=localfile.png                 \n"
	       " ----------------------------------------------------------------------------------\n"
	       "  --help                             Print this help page and exit                 \n"
	       "  --url=<url>                        The URL to capture (http:...|file:...|...)    \n"
	       "  --out=<path>                       The target file (.png|pdf|ps|svg|jpeg|...)    \n"
	       "  --out-format=<f>                   Like extension in --out, overrides heuristic  \n"
	       // "  --out-quality=<int>             Output format quality from 1 to 100           \n"
	       "  --min-width=<int>                  Minimal width for the image (default: 800)    \n"
	       "  --min-height=<int>                 Minimal height for the image (default: 600)   \n"
	       "  --force-gpu-mem-available-mb=<int> Set the memory in Chromium for rendering      \n"
	       "  --max-wait=<ms>                    Don't wait more than (default: 90000, inf: 0) \n"
	       "  --delay=<ms>                       After successful load, wait (default: 0)      \n"
	       // "  --user-styles=<url>             Location of user style sheet (deprecated)     \n"
	       // "  --user-style-path=<path>        Location of user style sheet file, if any
	       // (disabled until the insertion script is written) \n"
	       // "  --user-style-string=<css>       User style rules specified as text (disabled
	       // until the insertion script is      written) \n"
	       "  --header=<name>:<value>            request header; repeatable; some can't be set \n"
	       // "  --method=<get|post|put>            Specifies the request method (default: get)
	       // (disabled until a corresponding WebEngine setting is found) \n"
	       "  --body-string=<string>             Unencoded request body (default: none)        \n"
	       "  --body-base64=<base64>             Base64-encoded request body (default: none)   \n"
	       "  --app-name=<name>                  appName used in User-Agent; default is none   \n"
	       "  --app-version=<version>            appVers used in User-Agent; default is none   \n"
	       "  --user-agent=<string>              Override the User-Agent header Qt would set   \n"
	       "  --javascript=<on|off>              JavaScript execution (default: on)            \n"
	       // "  --java=<on|off>                 Java execution (default: unknown) (deprecated for
	       // WebEngine)  \n"
	       "  --plugins=<on|off>                 Plugin execution (default: unknown)           \n"
	       // "  --private-browsing=<on|off>    Private browsing (default: unknown) (disabled until
	       // corresponding WebEngine setting found)  \n"
	       "  --auto-load-images=<on|off>        Automatic image loading (default: on)         \n"
	       "  --js-can-open-windows=<on|off>     Script can open windows? (default: unknown)   \n"
	       "  --js-can-access-clipboard=<on|off> Script clipboard privs (default: unknown)     \n"
	       "  --print-backgrounds=<on|off>       Backgrounds in PDF/PS output (default: off)   \n"
	       "  --zoom-factor=<float>              Page zoom factor (default: no zooming)        \n"
	// "  --zoom-text-only=<on|off>       Whether to zoom only the text (default: off)
	// (deprecated for WebEngine) \n"
	// "  --http-proxy=<url>              Address for HTTP proxy server (default: none)
	// (disabled until corresponding WebEngine setting found)  \n"
#if CUTYCAPT_SCRIPT
	       "  --inject-script=<path>             JavaScript that will be injected into pages   \n"
	       "  --script-object=<string>           Property to hold state for injected script    \n"
	       "  --expect-alert=<string>            Try waiting for alert(string) before capture  \n"
	       "  --debug-print-alerts               Prints out alert(...) strings for debugging.  \n"
#endif
	       "  --smooth                           Attempt to enable Qt's high-quality settings. \n"
	       "  --insecure                         Ignore SSL/TLS certificate errors             \n"
	       " ----------------------------------------------------------------------------------\n"
	       "  <f> is svg,ps,pdf,itext,html,png,jpeg,mng,tiff,gif,bmp,ppm,xbm,xpm               \n"
	       " ----------------------------------------------------------------------------------\n"
#if CUTYCAPT_SCRIPT
	       " The `inject-script` option can be used to inject script code into loaded web      \n"
	       " pages. The code is called whenever the `javaScriptWindowObjectCleared` signal     \n"
	       " is received. When `script-object` is set, an object under the specified name      \n"
	       " will be available to the script to maintain state across page loads. When the     \n"
	       " `expect-alert` option is specified, the shot will be taken when a script in-      \n"
	       " vokes alert(string) with the string if that happens before `max-wait`. These      \n"
	       " options effectively allow you to remote control the browser and the web page.     \n"
	       " This an experimental and easily abused and misused feature. Use with caution.     \n"
	       " ----------------------------------------------------------------------------------\n"
#endif
	       " http://cutycapt.sf.net - (c) 2003-2013 Bjoern Hoehrmann - bjoern@hoehrmann.de     \n"
	       "");
}

int main(int argc, char* argv[]) {
	bool argHelp = false;
	int argDelay = 0;
	bool argSilent = false;
	bool argInsecure = false;
	int32_t argMinWidth = 800;
	int32_t argMinHeight = 600;
	uint32_t argMaxWait = 90000;
	uint8_t argVerbosity = 0;
	bool argSmooth = false;

	const char* argUrl = NULL;
	// const char* argUserStyle = NULL;
	// const char* argUserStylePath = NULL;
	// const char* argUserStyleString = NULL;
	// const char* argIconDbPath = NULL;
	const char* argInjectScript = NULL;
	const char* argScriptObject = NULL;
	QString argOut;

	CutyCapt::OutputFormat format = CutyCapt::OtherFormat;

	QApplication::setAttribute(Qt::AA_UseSoftwareOpenGL, true);
	QQuickWindow::setSceneGraphBackend(QSGRendererInterface::Software);

	QtWebEngine::initialize();

	QApplication app(argc, argv, true);

	CutyPage page;

	// QNetworkAccessManager::Operation method = QNetworkAccessManager::GetOperation;
	QByteArray body;
	QWebEngineHttpRequest req{};
	// QNetworkAccessManager manager;

	// Parse command line parameters
	for (int ax = 1; ax < argc; ++ax) {
		size_t nlen;

		const char* s = argv[ax];
		const char* value;

		// boolean options
		if (strcmp("--silent", s) == 0) {
			argSilent = true;
			continue;

		} else if (strcmp("--help", s) == 0) {
			argHelp = true;
			break;

		} else if (strcmp("--verbose", s) == 0) {
			argVerbosity++;
			continue;

		} else if (strcmp("--insecure", s) == 0) {
			argInsecure = true;
			continue;
		} else if (strcmp("--smooth", s) == 0) {
			argSmooth = 1;
			continue;

#if CUTYCAPT_SCRIPT
		} else if (strcmp("--debug-print-alerts", s) == 0) {
			page.setPrintAlerts(true);
			continue;
#endif
		}

		value = strchr(s, '=');

		if (value == NULL) {
			// TODO: error
			argHelp = 1;
			break;
		}

		nlen = value++ - s;

		// --name=value options
		if (strncmp("--url", s, nlen) == 0) {
			argUrl = value;
		} else if (strncmp("--min-width", s, nlen) == 0) {
			// TODO: add error checking here?
			argMinWidth = strtol(value, nullptr, 0);
		} else if (strncmp("--min-height", s, nlen) == 0) {
			// TODO: add error checking here?
			argMinHeight = strtol(value, nullptr, 0);
		} else if (strncmp("--force-gpu-mem-available-mb", s, nlen) == 0) {
			// don't actually do anything, just avoid this argument being treated as an error
		} else if (strncmp("--delay", s, nlen) == 0) {
			// TODO: see above
			argDelay = strtol(value, nullptr, 0);
		} else if (strncmp("--max-wait", s, nlen) == 0) {
			// TODO: see above
			argMaxWait = strtol(value, nullptr, 0);
		} else if (strncmp("--out", s, nlen) == 0) {
			argOut = value;

			if (format == CutyCapt::OtherFormat) {
				for (int ix = 0; CutyExtMap[ix].id != CutyCapt::OtherFormat; ++ix) {
					if (argOut.endsWith(CutyExtMap[ix].extension))
						format = CutyExtMap[ix].id; //, break;
				}
			}
		/* } else if (strncmp("--user-styles", s, nlen) == 0) {
      // This option is provided for backwards-compatibility only
      argUserStyle = value;
    } else if (strncmp("--user-style-path", s, nlen) == 0) {
      argUserStylePath = value;
    } else if (strncmp("--user-style-string", s, nlen) == 0) {
      argUserStyleString = value;
    } else if (strncmp("--icon-database-path", s, nlen) == 0) {
      argIconDbPath = value;
		*/ } else if (strncmp("--auto-load-images", s, nlen) == 0) {
			page.setAttribute(QWebEngineSettings::AutoLoadImages, value);
		} else if (strncmp("--javascript", s, nlen) == 0) {
			page.setAttribute(QWebEngineSettings::JavascriptEnabled, value);
		} /* else if (strncmp("--java", s, nlen) == 0) {
		  page.setAttribute(QWebEngineSettings::JavaEnabled, value);
		} */
		else if (strncmp("--plugins", s, nlen) == 0) {
			page.setAttribute(QWebEngineSettings::PluginsEnabled, value);
		} /* else if (strncmp("--private-browsing", s, nlen) == 0) {
		  page.setAttribute(QWebEngineSettings::PrivateBrowsingEnabled, value);
		} */
		else if (strncmp("--js-can-open-windows", s, nlen) == 0) {
			page.setAttribute(QWebEngineSettings::JavascriptCanOpenWindows, value);
		} else if (strncmp("--js-can-access-clipboard", s, nlen) == 0) {
			page.setAttribute(QWebEngineSettings::JavascriptCanAccessClipboard, value);
		} /* else if (strncmp("--developer-extras", s, nlen) == 0) {
		  page.setAttribute(QWebEngineSettings::DeveloperExtrasEnabled, value);
		} */
		else if (strncmp("--links-included-in-focus-chain", s, nlen) == 0) {
			page.setAttribute(QWebEngineSettings::LinksIncludedInFocusChain, value);
		} else if (strncmp("--print-backgrounds", s, nlen) == 0) {
			page.setAttribute(QWebEngineSettings::PrintElementBackgrounds, value);
		} else if (strncmp("--zoom-factor", s, nlen) == 0) {
			page.setZoomFactor(static_cast<qreal>(QString(value).toFloat()));
			/* }  else if (strncmp("--zoom-text-only", s, nlen) == 0) {
			  page.setAttribute(QWebEngineSettings::ZoomTextOnly, value);
			*//* } else if (strncmp("--http-proxy", s, nlen) == 0) {
      QUrl p = QUrl::fromEncoded(value);
		  QNetworkProxy proxy =
		      QNetworkProxy(QNetworkProxy::HttpProxy, p.host(), p.port(80), p.userName(), p.password());
      manager.setProxy(proxy);
      page.setNetworkAccessManager(&manager);
		*/

#if CUTYCAPT_SCRIPT
		} else if (strncmp("--inject-script", s, nlen) == 0) {
			argInjectScript = value;
		} else if (strncmp("--script-object", s, nlen) == 0) {
			argScriptObject = value;
		} else if (strncmp("--expect-alert", s, nlen) == 0) {
			page.setAlertString(value);
#endif

		} else if (strncmp("--app-name", s, nlen) == 0) {
			app.setApplicationName(value);
		} else if (strncmp("--app-version", s, nlen) == 0) {
			app.setApplicationVersion(value);
		} else if (strncmp("--body-base64", s, nlen) == 0) {
			body = QByteArray::fromBase64(value);
		} else if (strncmp("--body-string", s, nlen) == 0) {
			body = QByteArray(value);
		} else if (strncmp("--user-agent", s, nlen) == 0) {
			page.setUserAgent(value);
		} else if (strncmp("--out-format", s, nlen) == 0) {
			for (int ix = 0; CutyExtMap[ix].id != CutyCapt::OtherFormat; ++ix) {
				if (strcmp(value, CutyExtMap[ix].identifier) == 0)
					format = CutyExtMap[ix].id; //, break;
			}

			if (format == CutyCapt::OtherFormat) {
				// TODO: error
				argHelp = true;
				break;
			}
		} else if (strncmp("--header", s, nlen) == 0) {
			const char* hv = strchr(value, ':');

			if (!hv) {
				// TODO: error
				argHelp = true;
				break;
			}

			req.setHeader(QByteArray(value, hv - value), hv + 1);
		} /* else if (strncmp("--method", s, nlen) == 0) {
		  if (strcmp("value", "get") == 0)
		    method = QNetworkAccessManager::GetOperation;
		  else if (strcmp("value", "put") == 0)
		    method = QNetworkAccessManager::PutOperation;
		  else if (strcmp("value", "post") == 0)
		    method = QNetworkAccessManager::PostOperation;
		  else if (strcmp("value", "head") == 0)
		    method = QNetworkAccessManager::HeadOperation;
		  else
		    (void)0; // TODO: ...
		} */
		else {
			// TODO: error
			argHelp = true;
		}
	}

	if (argUrl == NULL || argOut == NULL || argHelp) {
		CaptHelp();
		return EXIT_FAILURE;
	}

	if (!argSilent) {
		std::clog << "pixmap maximum dimension: " << INT_MAX << "x" << INT_MAX << std::endl;
	}

	// This used to use QUrl(argUrl) but that escapes %hh sequences
	// even though it should not, as URLs can assumed to be escaped.
	req.setUrl(QUrl::fromEncoded(argUrl));

	QString scriptProp(argScriptObject);
	QString scriptCode;

	if (argInjectScript) {
		QFile file(argInjectScript);
		if (file.open(QIODevice::ReadOnly)) {
			QTextStream stream(&file);
			stream.setCodec(QTextCodec::codecForName("UTF-8"));
			stream.setAutoDetectUnicode(true);
			scriptCode = stream.readAll();
			file.close();
		}
	}

	CutyCapt main{ &page,      argOut,      argDelay,  format,   scriptProp,
		             scriptCode, argInsecure, argSmooth, argSilent };

	app.connect(&page, SIGNAL(loadFinished(bool)), &main, SLOT(DocumentComplete(bool)));

	// Qt WebEngine removes the ability to check whether a page has layout it's components
	// This checking functionality needs to be rewritten in JavaScript, and potentially Qt websockets
	// / channels

	// app.connect(page, SIGNAL(initialLayoutCompleted()), &main, SLOT(InitialLayoutCompleted()));

	app.connect(page.page(), SIGNAL(contentsSizeChanged(const QSizeF&)), &main,
	            SLOT(onSizeChanged(const QSizeF&)));

	if (argMaxWait > 0) {
		// TODO: Should this also register one for the application?
		QTimer::singleShot(argMaxWait, &main, SLOT(Timeout()));
	}

	/*
	if (argUserStyle != NULL)
	  // TODO: does this need any syntax checking?
	  page.settings()->setUserStyleSheetUrl(QUrl::fromEncoded(argUserStyle));

	if (argUserStylePath != NULL) {
	  page.settings()->setUserStyleSheetUrl(QUrl::fromLocalFile(argUserStylePath));
	}

	if (argUserStyleString != NULL) {
	  QUrl data("data:text/css;charset=utf-8;base64," + QByteArray(argUserStyleString).toBase64());
	  page.settings()->setUserStyleSheetUrl(data);
	}

	if (argIconDbPath != NULL)
	  // TODO: does this need any syntax checking?
	  page.settings()->setIconDatabasePath(argUserStyle);
	*/

	page.setAttribute(QWebEngineSettings::WebAttribute::ShowScrollBars, "off");
	page.setAttribute(Qt::WA_DontShowOnScreen, true);

#if CUTYCAPT_SCRIPT
	// javaScriptWindowObjectCleared does not get called on the
	// initial load unless some JavaScript has been executed.
	page->runJavaScript(QString(""));

	app.connect(page, SIGNAL(javaScriptWindowObjectCleared()), &main,
	            SLOT(JavaScriptWindowObjectCleared()));
#endif

	if (!body.isNull())
		req.setPostData(body);

	page.load(req);

	QSize argSize(argMinWidth, argMinHeight);

	page.setMinimumSize(argSize);
	page.setMaximumSize(QSize{ QWIDGETSIZE_MAX, QWIDGETSIZE_MAX });
	page.resize(argSize);
	page.show();

	return app.exec();
}
