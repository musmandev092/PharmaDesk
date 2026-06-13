#pragma once

#include <QString>

// App-wide theming. Themes are QSS files under :/themes/<key>.qss; the empty key
// (or any missing file) uses the base :/theme.qss. To add a theme, drop a new
// resources/themes/<key>.qss, register it in resources.qrc, and add it to the
// picker in AdminPage.
namespace Theme {

// Apply the theme with the given key to the whole application (live).
void apply(const QString &key);

} // namespace Theme
