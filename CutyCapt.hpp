#include <QtWebEngine>

#if QT_VERSION >= 0x050000
#	include <QtWebEngineWidgets>
#endif

class CutyCapt;
class CutyPage : public QWebEngineView {
	Q_OBJECT

public:
	void setAttribute(QWebEngineSettings::WebAttribute option, const QString& value);
	void setAttribute(Qt::WidgetAttribute option, const bool value);
	void setUserAgent(const QString& userAgent);
	void setAlertString(const QString& alertString);
	void setPrintAlerts(bool printAlerts);
	void setCutyCapt(CutyCapt* cutyCapt);
	QString getAlertString();

protected:
	QString chooseFile(QWebEnginePage* frame, const QString& suggestedFile);
	void javaScriptConsoleMessage(const QString& message, int lineNumber, const QString& sourceID);
	bool javaScriptPrompt(QWebEnginePage* frame, const QString& msg, const QString& defaultValue,
	                      QString* result);
	void javaScriptAlert(QWebEnginePage* frame, const QString& msg);
	bool javaScriptConfirm(QWebEnginePage* frame, const QString& msg);
	QString userAgentForUrl(/* const QUrl& url */) const;

	QString mUserAgent;
	QString mAlertString;
	bool mPrintAlerts;
	CutyCapt* mCutyCapt;
};

class CutyCapt : public QObject {
	Q_OBJECT

public:
	// TODO: This should really be elsewhere and be named differently
	enum OutputFormat {
		SvgFormat,
		PdfFormat,
		PsFormat,
		InnerTextFormat,
		HtmlFormat,
		PngFormat,
		JpegFormat,
		MngFormat,
		TiffFormat,
		GifFormat,
		BmpFormat,
		PpmFormat,
		XbmFormat,
		XpmFormat,
		OtherFormat
	};

	CutyCapt(CutyPage* page, const QString& output, int delay, OutputFormat format,
	         const QString& scriptProp, const QString& scriptCode, bool insecure, bool smooth,
	         bool silent);

private slots:
	void DocumentComplete(bool ok);
	void InitialLayoutCompleted();
	void JavaScriptWindowObjectCleared();
	void Delayed();
	void onSizeChanged(const QSizeF& size);

public slots:
	void Timeout();
	void pdfPrintFinish(const QString&, bool success);

private:
	void TryDelayedRender();
	void saveSnapshot();
	bool mSawInitialLayout;
	bool mSawDocumentComplete;
	bool mSawGeometryChange;
	QSize mViewSize;

protected:
	QString mOutput;
	int mDelay;
	CutyPage* mPage;
	OutputFormat mFormat;
	QObject* mScriptObj;
	QString mScriptProp;
	QString mScriptCode;
	bool mInsecure;
	bool mSmooth;
	bool mSilent;

public:
	QTimer mTimeoutTimer;
};
