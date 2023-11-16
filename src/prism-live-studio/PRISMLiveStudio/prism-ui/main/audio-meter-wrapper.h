#pragma once

class PLSAudioMeterWrapper {
public:
	virtual ~PLSAudioMeterWrapper() = default;

	virtual QWidget *GetWidget() = 0;
};
