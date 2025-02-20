#include "ChannelsSettingsWidget.h"
#include <QCloseEvent>
#include <functional>
#include "ChannelCommonFunctions.h"
#include "ChannelItem.h"
#include "LogPredefine.h"
#include "PLSChannelsVirualAPI.h"
#include "PLSComboBox.h"
#include "PLSMessageBox.h"
#include "PLSSyncServerManager.hpp"
#include "ui_ChannelsSettingsWidget.h"
using namespace item_data_role;

ChannelsSettingsWidget::ChannelsSettingsWidget(QWidget *parent) : PLSDialogView(parent), ui(new Ui::ChannelsSettingsWidget)
{
	//dpiHelper.setCss(this, {PLSCssIndex::ChannelsSettingsWidget});
	setResizeEnabled(false);
	//setIsMoveInContent(true);
	pls_add_css(this, {"ChannelsSettingsWidget"});

	setupUi(ui);
	initSize(775, 482);
	this->initializePlatforms(getDefaultPlatforms());
#if defined(Q_OS_MACOS)
	this->setHasCaption(true);
	this->setWindowTitle(tr("Channels.SettingsMainTitle"));
	this->setHasCloseButton(false);
	ui->horizontalLayout_2->insertWidget(2, ui->ApplySettingsBtn);
#else
	this->setHasCaption(false);
#endif
	ui->CenterStack->setFocus();
	ui->LogoLabel->setScaledContents(true);
	connect(PLSCHANNELS_API, &PLSChannelDataAPI::channelModified, this, &ChannelsSettingsWidget::updateChannelUi, Qt::QueuedConnection);
}

ChannelsSettingsWidget::~ChannelsSettingsWidget()
{
	delete ui;
}

void ChannelsSettingsWidget::setChannelsData(const ChannelsMap &datas, const QString &platform)
{
	auto temp = getDefaultPlatforms();
	removeCustomRtmpInComboxList(temp);

	int index = static_cast<int>(temp.indexOf(platform)) + 1;
	this->setChannelsData(datas, index);
}

void ChannelsSettingsWidget::setChannelsData(const ChannelsMap &datas, int index)
{
	mOriginalInfo = datas;
	mLastChannelsInfo = datas;
	auto ite = datas.begin();
	for (; ite != datas.end(); ++ite) {
		const auto &var = ite.value();
		auto item = new ChannelItem(this);
		item->setData(var);
		int order = getInfo(var, ChannelData::g_displayOrder, int(0));
		QString orderStr = QString::number(order).rightJustified(10, '0');
		auto listItem = new QListWidgetItem(orderStr, ui->channelsListWidget);
		listItem->setData(ChannelItemData, var);
		ui->channelsListWidget->addItem(listItem);
		ui->channelsListWidget->setItemWidget(listItem, item);
		m_items.insert(var.value(ChannelData::g_channelUUID).toString(), item);
		connect(item, &ChannelItem::sigSelectionChanged, this, &ChannelsSettingsWidget::onSelectionChanged);
	}
	ui->channelsListWidget->setItemDelegate(new GeometryDelegate(this));
	ui->channelsListWidget->sortItems();
	ui->ChannelsListCombox->setCurrentIndex(index);
}

void ChannelsSettingsWidget::initializePlatforms(const QStringList &platforms)
{

	QStringList tmp = platforms;
	removeCustomRtmpInComboxList(tmp);

	tmp.prepend(CHANNELS_TR(ALL));
	for (int i = 0; i < tmp.size(); ++i) {
		const auto &platform = tmp[i];
		auto displatPlatform = translatePlatformName(platform);
		ui->ChannelsListCombox->addItem(displatPlatform);
		ui->ChannelsListCombox->setItemData(i, platform, ChannelItemData);
	}

	ui->ChannelsListCombox->setMaxVisibleItems(static_cast<int>(tmp.size()));

	auto pushButton = new QPushButton(ui->ChannelsListCombox);
	auto layout = new QHBoxLayout();
	layout->addWidget(pushButton);
	layout->setContentsMargins(0, 0, 0, 0);
	ui->ChannelsListCombox->setLayout(layout);
	pushButton->setText(ui->ChannelsListCombox->currentText());
	pushButton->setCheckable(true);
	connect(pushButton, &QPushButton::clicked, ui->ChannelsListCombox, &PLSComboBox::showPopup);
	connect(ui->ChannelsListCombox, &QComboBox::currentTextChanged, pushButton, &QPushButton::setText);
	connect(ui->ChannelsListCombox, &PLSComboBox::popupShown, pushButton, &QPushButton::setChecked);
}

void ChannelsSettingsWidget::changeEvent(QEvent *e)
{
	QWidget::changeEvent(e);
	switch (e->type()) {
	case QEvent::LanguageChange:
		ui->retranslateUi(this);
		break;
	case QEvent::Show:
		break;
	default:
		break;
	}
}

void ChannelsSettingsWidget::closeEvent(QCloseEvent *event)
{
	if (!mLastChannelsInfo.isEmpty() && hasSelected()) {
		event->ignore();
		return;
	}
	applyChanges();
	event->accept();
}

bool ChannelsSettingsWidget::hasSelected() const
{
	auto isSelected = [](const QVariantMap &info) { return getInfo(info, ChannelData::g_displayState, true); };

	if (auto ite = std::find_if(mLastChannelsInfo.cbegin(), mLastChannelsInfo.cend(), isSelected); ite != mLastChannelsInfo.cend()) {
		return true;
	}
	return false;
}

void ChannelsSettingsWidget::applyChanges()
{
	m_bClose = true;
	HolderReleaser releaser(&PLSChannelDataAPI::holdOnChannelArea, PLSCHANNELS_API);
	auto lstIte = mLastChannelsInfo.cbegin();
	for (; lstIte != mLastChannelsInfo.cend(); ++lstIte) {
		QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
		const auto &varMap = lstIte.value();

		if (auto orginalInfo = mOriginalInfo.value(lstIte.key()); getInfo(varMap, ChannelData::g_displayState, true) == getInfo(orginalInfo, ChannelData::g_displayState, true)) {
			continue;
		}
		PLSCHANNELS_API->setChannelInfos(varMap, false);
		auto uuid = getInfo(varMap, ChannelData::g_channelUUID, QString());
		PLSCHANNELS_API->addChannelForDashBord(uuid);
	}
}

void ChannelsSettingsWidget::removeCustomRtmpInComboxList(QStringList &platforms)
{
	auto supportMap = PLSSyncServerManager::instance()->getSupportedPlatformsMap();
	foreach(QVariant value, supportMap.values())
	{
		auto channelMap = value.toMap();
		auto platform = channelMap.value("platform").toString();
		if (platform == CUSTOM_RTMP) {
			auto key = supportMap.key(channelMap);
			platforms.removeOne(key);
		}
	}
}

template<typename FuncType, typename ListType> void checkItemVisibility(int rowCount, const FuncType &fun, const ListType &listWidget, int &visibleCount)
{
	for (int i = 0; i < rowCount; ++i) {
		auto item = listWidget->item(i);
		bool toHide = fun(item);
		item->setHidden(toHide);
		if (!toHide) {
			++visibleCount;
		}
	}
}

void ChannelsSettingsWidget::on_ChannelsListCombox_currentTextChanged(const QString &)
{
	auto platform = ui->ChannelsListCombox->currentData(ChannelItemData).toString();
	PRE_LOG_UI_MSG(QString("Channels Filter changed to " + platform).toStdString().c_str(), ChannelsSettingsWidget)
	int rowCount = ui->channelsListWidget->count();
	if (rowCount == 0) {
		return;
	}
	int visibleCount = 0;
	int type = ChannelData::ChannelType;
	if (ui->ChannelsListCombox->currentIndex() == (ui->ChannelsListCombox->count() - 1)) {
		type = int(ChannelData::RTMPType);
	} else if (ui->ChannelsListCombox->currentIndex() == 0) {
		type = int(ChannelData::NoType);
	}
	// to check visibility of item

	if (type >= ChannelData::CustomType) {

		auto isNotRTMP = [](const QListWidgetItem *item) {
			auto varMap = item->data(ChannelItemData).toMap();
			int tmpType = getInfo(varMap, ChannelData::g_data_type, ChannelData::NoType);
			return (tmpType == ChannelData::ChannelType);
		};

		checkItemVisibility(rowCount, isNotRTMP, ui->channelsListWidget, visibleCount);
	}

	if (type == ChannelData::ChannelType) {
		auto isNotMatchPlatform = [&platform](const QListWidgetItem *item) {
			auto varMap = item->data(ChannelItemData).toMap();
			int tmpType = getInfo(varMap, ChannelData::g_data_type, ChannelData::NoType);
			QString platformName = getInfo(varMap, ChannelData::g_channelName);
			return (!platformName.contains(platform, Qt::CaseInsensitive) || tmpType != ChannelData::ChannelType);
		};

		checkItemVisibility(rowCount, isNotMatchPlatform, ui->channelsListWidget, visibleCount);
	}

	if (type == ChannelData::NoType) {
		auto allNotHide = [](const QListWidgetItem *) { return false; };
		checkItemVisibility(rowCount, allNotHide, ui->channelsListWidget, visibleCount);
	}

	if (visibleCount == 0) {
		ui->CenterStack->setCurrentIndex(1);
		setGuidePageInfo(platform);
	} else {
		ui->CenterStack->setCurrentIndex(0);
	}
}

void ChannelsSettingsWidget::onSelectionChanged(const QString &uuid, bool isSelected)
{
	auto ite = mLastChannelsInfo.find(uuid);
	if (ite != mLastChannelsInfo.end()) {
		auto &varMap = ite.value();
		varMap.insert(ChannelData::g_displayState, isSelected);
		if (!isSelected) {
			varMap.insert(ChannelData::g_channelUserStatus, ChannelData::Disabled);
			varMap.insert(ChannelData::g_channelDualOutput, ChannelData::NoSet);
		}
	}
}

void ChannelsSettingsWidget::on_ApplySettingsBtn_clicked()
{
	if (!hasSelected()) {
		PLSMessageBox::warning(this, tr("Alert.Title"), CHANNELS_TR(NoChannelSelected));
		return;
	}
	PRE_LOG_UI_MSG("try apply changes  ", ChannelsSettingsWidget)
	applyChanges();
	this->accept();
}

void ChannelsSettingsWidget::on_Cancel_clicked()
{
	PRE_LOG_UI_MSG("cancel   ", ChannelsSettingsWidget)
	this->reject();
}

void ChannelsSettingsWidget::on_GotoLoginBtn_clicked()
{
	if (!hasSelected()) {
		PLSAlertView::warning(this, tr("Alert.Title"), CHANNELS_TR(AtLeastOneSelected));
		return;
	}
	PRE_LOG_UI_MSG("try to go to login page   ", ChannelsSettingsWidget)
	applyChanges();
	this->accept();
	QString cmd = ui->ChannelsListCombox->currentData(ChannelItemData).toString();
	runCMD(cmd);
}

void ChannelsSettingsWidget::setGuidePageInfo(const QString &platfom)
{
	QString guidInfo;
	QString guidBtnText;
	if (ui->ChannelsListCombox->currentIndex() == ui->ChannelsListCombox->count() - 1) {
		guidInfo = CHANNELS_TR(GuideRTM);
		guidBtnText = CHANNELS_TR(GotoLoginRTMP);
	} else {
		guidInfo = CHANNELS_TR(GuideInfos);
		guidBtnText = CHANNELS_TR(GotoLogin);
	}

	ui->GuideLabel->setText(guidInfo);
	ui->GotoLoginBtn->setText(guidBtnText);

	auto pixPath = getPlatformImageFromName(platfom, channel_data::ImageType::channelSettingBigIcon, "addch-", "-large");
	QPixmap pix;
	auto size = ui->LogoLabel->size();
	loadPixmap(pix, pixPath, size * 4);
	ui->LogoLabel->setPixmap(pix);
}

void ChannelsSettingsWidget::updateChannelUi(const QString &uuid)
{
	if (m_bClose)
		return;
	if (m_items.find(uuid) != m_items.end()) {
		auto item = m_items.value(uuid);
		if (item) {
			auto data = PLSCHANNELS_API->getChannelInfo(uuid);
			item->setData(data);
		}
	}
	mLastChannelsInfo = PLSCHANNELS_API->getAllChannelInfo();
}
