pragma Singleton

import QtQuick

// Direction artistique du §5.7, en un seul endroit : aucune couleur, aucun
// rayon et aucune durée ne doivent être écrits en littéral ailleurs.
QtObject {
    readonly property color bg: "#16171A"
    readonly property color surface: "#1E2024"
    readonly property color surfaceAlt: "#23262B"
    readonly property color border: "#2C2F36"
    readonly property color text: "#E8E9EC"
    readonly property color textDim: "#8B8F99"
    readonly property color accent: "#5B8DEF"
    readonly property color accentDim: "#3A5EA8"
    readonly property color ok: "#4ADE80"
    readonly property color warn: "#FBBF24"
    readonly property color danger: "#F87171"

    readonly property color hover: Qt.rgba(1, 1, 1, 0.05)
    readonly property color pressed: Qt.rgba(1, 1, 1, 0.09)
    readonly property color selected: Qt.rgba(0.357, 0.553, 0.937, 0.16)

    // Échelle d'espacement imposée : 4 / 8 / 12 / 16 / 24 / 32.
    readonly property int s1: 4
    readonly property int s2: 8
    readonly property int s3: 12
    readonly property int s4: 16
    readonly property int s5: 24
    readonly property int s6: 32

    readonly property int radiusControl: 6
    readonly property int radiusCard: 10

    readonly property int hoverMs: 150
    readonly property int sectionMs: 200
    readonly property int gaugeMs: 400

    readonly property string fontUi: App.uiFontFamily
    readonly property string fontMono: App.monoFontFamily
    readonly property int fontSize: 13
    readonly property int fontSizeSmall: 11
    readonly property int fontSizeTitle: 15

    readonly property int controlHeight: 30
    readonly property int labelWidth: 230
    readonly property int tooltipDelay: 300
    readonly property int tooltipMaxWidth: 360
}
