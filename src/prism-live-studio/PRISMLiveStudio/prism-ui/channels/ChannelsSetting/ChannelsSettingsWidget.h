#ifndef CHANNELSSETTINGSWIDGET_H
#define CHANNELSSETTINGSWIDGET_H
#include <QSharedPointer>
#include <QStyledItemDelegate>
#include <QVariantMap>
#include <QWidget>
#include "PLSChannelDataAPI.h"
#include "PLSDialogView.h"

namespace Ui {
class ChannelsSettingsWidget;
}
namespace item_data_role {
static const int ChannelItemData = Qt::UserRole + 1; //
}

using WidgetMap = QMap<QString, QWidget *>;

class ChannelsSettingsWidget : public PLSDialogView {
	Q_OBJECT

public:
	explicit ChannelsSettingsWidget(QWidget *parent = nullptr);
	~ChannelsSettingsWidget() override;

	void setChannelsData(const ChannelsMap &datas, const QString &platform = "");

	void setChannelsData(const ChannelsMap &datas, int Index = 0);

	void initializePlatforms(const QStringList &platforms);

public slots:
	void on_ChannelsListCombox_currentTextChanged(const QString &);
	void onSelectionChanged(const QString &uuid, bool isSelected);
	void on_ApplySettingsBtn_clicked();
	void on_Cancel_clicked();
	void on_GotoLoginBtn_clicked();
	void setGuidePageInfo(const QString &platfom);

protected:
	void changeEvent(QEvent *e) override;
	void closeEvent(QCloseEvent *event) override;

private:
	bool hasSelected() const;
	void applyChanges() const;

	//private:
	Ui::ChannelsSettingsWidget *ui;
	ChannelsMap mLastChannelsInfo;
	ChannelsMap mOriginalInfo;
};

class GeometryDelegate : public QStyledItemDelegate {
	Q_OBJECT
public:
	explicit GeometryDelegate(QWidget *parent = nullptr) : QStyledItemDelegate(parent) {}

	void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &) const override
	{
		auto itemRec = option.rect;
		auto itemCenter = itemRec.center();
		QRect editorRec = editor->geometry();
		editorRec.moveCenter(itemCenter);
		editor->setGeometry(editorRec);
	}
};

#endif // CHANNELSSETTINGSWIDGET_H
