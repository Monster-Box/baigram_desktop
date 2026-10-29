/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "intro/intro_step.h"
#include "ui/widgets/fields/input_field.h"

namespace Ui {
class FlatLabel;
class LinkButton;
class RoundButton;
} // namespace Ui

namespace Core {
struct ServerConfig;
} // namespace Core

namespace Intro {
namespace details {

class StartWidget : public Step {
public:
	StartWidget(
		QWidget *parent,
		not_null<Main::Account*> account,
		not_null<Data*> data);

	void submit() override;
	rpl::producer<QString> nextButtonText() const override;
	rpl::producer<> nextButtonFocusRequests() const override;
	void activate() override;
	void setInnerFocus() override;
	[[nodiscard]] int coverTextShift() const override;

protected:
	void resizeEvent(QResizeEvent *e) override;

private:
	void createSticker();
	void updateStickerGeometry();
	void updateControlsGeometry();
	void submitServerId(const QString &id);
	void serverConfigDone(const Core::ServerConfig &config);
	void serverConfigFail(const QString &error);

	rpl::event_stream<> _nextButtonFocusRequests;
	object_ptr<Ui::RpWidget> _sticker = { nullptr };
	object_ptr<Ui::InputField> _idInput = { nullptr };
	rpl::variable<QString> _nextText;
	bool _requesting = false;

};

} // namespace details
} // namespace Intro
