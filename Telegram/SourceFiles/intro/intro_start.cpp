/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "intro/intro_start.h"

#include "lang/lang_keys.h"
#include "intro/intro_qr.h"
#include "intro/intro_phone.h"
#include "ui/painter.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/labels.h"
#include "main/main_account.h"
#include "main/main_app_config.h"
#include "core/server_config.h"
#include "styles/style_intro.h"
#include <lottie/lottie_icon.h>
#include <crl/crl_on_main.h>

#include <QtCore/QPointer>

namespace Intro {
namespace details {

StartWidget::StartWidget(
		QWidget *parent,
		not_null<Main::Account*> account,
		not_null<Data*> data)
: Step(parent, account, data, true)
, _idInput(this, st::introName, rpl::single(u"Server ID"_q))
, _nextText(tr::lng_start_msgs(tr::now)) {
	setMouseTracking(true);
	setTitleText(rpl::single(u"Baigram Desktop"_q));
	setDescriptionText(tr::lng_intro_about());
	createSticker();
	show();
}

void StartWidget::createSticker() {
	_sticker = object_ptr<Ui::RpWidget>(this);
	const auto raw = _sticker.data();
	const auto size = QSize(st::introStickerSize, st::introStickerSize);
	raw->resize(size);
	const auto icon = raw->lifetime().make_state<Lottie::Icon>(
		Lottie::IconDescriptor{
			.name = u"intro_sticker"_q,
			.path = u":/animations/baigram_sticker.tgs"_q,
			.sizeOverride = size,
			.limitFps = true,
		});
	struct State {
		bool started = false;
	};
	const auto state = raw->lifetime().make_state<State>();
	const auto start = [=] {
		const auto count = icon->framesCount();
		if (count > 1) {
			icon->animate([=] { raw->update(); }, 0, count - 1);
		}
	};
	raw->paintRequest(
	) | rpl::on_next([=] {
		auto p = QPainter(raw);
		icon->paintInCenter(p, raw->rect());
		if (!icon->animating()
			&& icon->framesCount() > 1
			&& (!state->started || icon->frameIndex() > 0)) {
			state->started = true;
			crl::on_main(raw, start);
		}
	}, raw->lifetime());
}

int StartWidget::coverTextShift() const {
	return (width() >= st::introStickerMinWidth)
		? st::introCoverTextShift
		: 0;
}

void StartWidget::resizeEvent(QResizeEvent *e) {
	Step::resizeEvent(e);
	updateStickerGeometry();
	updateControlsGeometry();
}

void StartWidget::updateStickerGeometry() {
	if (!_sticker) {
		return;
	}
	const auto shift = coverTextShift();
	_sticker->setVisible(shift > 0);
	_sticker->moveToLeft(
		(width() - _sticker->width()) / 2 - shift,
		contentTop() + 4);
}

void StartWidget::updateControlsGeometry() {
	if (!_idInput) {
		return;
	}
	const auto shift = coverTextShift();
	_idInput->moveToLeft(
		(width() - _idInput->width()) / 2 + shift,
		contentTop() + 132);
}

void StartWidget::submit() {
	if (_requesting) {
		return;
	}
	const auto id = _idInput->getLastText().trimmed();
	if (id.isEmpty()) {
		showError(rpl::single(u"Please enter a server ID"_q));
		_idInput->showError();
		_idInput->setFocus();
		return;
	}
	hideError();
	account().destroyStaleAuthorizationKeys();
	_requesting = true;
	_nextText = u"Loading…"_q;
	submitServerId(id);
}

void StartWidget::submitServerId(const QString &id) {
	const auto guard = QPointer<StartWidget>(this);
	Core::ServerConfigManager::Instance().request(
		id,
		[=](const Core::ServerConfig &config) {
			if (guard) {
				guard->serverConfigDone(config);
			}
		},
		[=](const QString &error) {
			if (guard) {
				guard->serverConfigFail(error);
			}
		});
}

void StartWidget::serverConfigDone(const Core::ServerConfig &config) {
	_requesting = false;
	Core::SaveServerBinding(&account(), config);
	Core::ApplyServerBindingToAccount(&account(), config);
	goNext<PhoneWidget>();
}

void StartWidget::serverConfigFail(const QString &error) {
	_requesting = false;
	_nextText = tr::lng_start_msgs(tr::now);
	showError(rpl::single(error));
	_idInput->showError();
	_idInput->setFocus();
}

rpl::producer<QString> StartWidget::nextButtonText() const {
	return _nextText.value();
}

rpl::producer<> StartWidget::nextButtonFocusRequests() const {
	return _nextButtonFocusRequests.events();
}

void StartWidget::activate() {
	Step::activate();
	showChildren();
	setInnerFocus();
}

void StartWidget::setInnerFocus() {
	_idInput->setFocusFast();
}

} // namespace details
} // namespace Intro
