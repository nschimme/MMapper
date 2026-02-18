// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "UrlUtils.h"

#include <QDesktopServices>
#include <QUrl>

#ifdef Q_OS_WASM
#include <emscripten.h>
#endif

namespace mmqt {

#ifdef Q_OS_WASM
EM_JS(void, open_url_js, (const char *url), {
    window.open(UTF8ToString(url), '_blank');
});
#endif

void openUrl(const QUrl &url)
{
#ifdef Q_OS_WASM
    open_url_js(url.toString().toUtf8().constData());
#else
    QDesktopServices::openUrl(url);
#endif
}

} // namespace mmqt
