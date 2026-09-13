#pragma once

#include <QString>

namespace ui::Fonts {

/// Enregistre les polices embarquées (§5.7). Idempotent.
void install();

/// Famille d'interface : Inter, ou la police système la plus proche si la
/// ressource embarquée manque.
QString uiFamily();

/// Famille à espacement fixe : JetBrains Mono, ou un repli système.
QString monoFamily();

} // namespace ui::Fonts
