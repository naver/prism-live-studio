#include "PLSAudioMixerScrollBar.h"

PLSAudioMixerScrollBar::PLSAudioMixerScrollBar() {}
PLSAudioMixerScrollBar::~PLSAudioMixerScrollBar() {}

void PLSAudioMixerScrollBar::showEvent(QShowEvent *event)
{
	emit isShowScrollBar(true);
	QScrollBar::showEvent(event);
}

void PLSAudioMixerScrollBar::hideEvent(QHideEvent *event)
{
	emit isShowScrollBar(false);
	QScrollBar::hideEvent(event);
}
