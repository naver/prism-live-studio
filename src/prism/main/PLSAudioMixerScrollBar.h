#ifndef PLSAUDIOMIXERSCROLLBAR_H
#define PLSAUDIOMIXERSCROLLBAR_H

#include <QObject>
#include <qscrollbar.h>

class PLSAudioMixerScrollBar : public QScrollBar {
	Q_OBJECT
public:
	PLSAudioMixerScrollBar();
	~PLSAudioMixerScrollBar();

	// QWidget interface
protected:
	void showEvent(QShowEvent *event);

	// QWidget interface
protected:
	void hideEvent(QHideEvent *event);
signals:
	void isShowScrollBar(bool isShow);
};

#endif // PLSAUDIOMIXERSCROLLBAR_H
