import QtQuick
import QtQuick.Controls.Basic

Button {
    id: control

    // normal | primary | danger | ghost
    property string variant: "normal"

    readonly property color _baseColor: {
        if (variant === "primary")
            return Theme.accent
        if (variant === "ghost")
            return "transparent"
        return Theme.surfaceAlt
    }
    readonly property color _textColor: {
        if (!enabled)
            return Theme.textDim
        if (variant === "primary")
            return "#0E1117"
        if (variant === "danger")
            return Theme.danger
        return Theme.text
    }

    implicitHeight: Theme.controlHeight
    // Le libellé n'est pas élidé : texte élidé et largeur implicite du bouton
    // s'alimentent mutuellement et le libellé finit rogné d'un pixel.
    implicitWidth: Math.max(72, label.implicitWidth + 2 * Theme.s4)
    hoverEnabled: true
    padding: Theme.s3

    contentItem: Text {
        id: label
        text: control.text
        color: control._textColor
        font.family: Theme.fontUi
        font.pixelSize: Theme.fontSize
        font.weight: control.variant === "primary" ? Font.DemiBold : Font.Normal
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    background: Rectangle {
        radius: Theme.radiusControl
        color: {
            if (!control.enabled)
                return control.variant === "ghost" ? "transparent" : Theme.surface
            if (control.pressed)
                return control.variant === "primary" ? Theme.accentDim
                                                     : Qt.lighter(control._baseColor, 1.35)
            if (control.hovered)
                return control.variant === "primary" ? Qt.lighter(Theme.accent, 1.1)
                                                     : Qt.lighter(control._baseColor, 1.2)
            return control._baseColor
        }
        // Le primaire désactivé garde une bordure, sinon il disparaît sur la barre.
        border.width: (control.variant === "primary" && control.enabled) ? 0 : 1
        border.color: control.enabled ? Theme.border : Qt.darker(Theme.border, 1.2)

        Behavior on color {
            ColorAnimation {
                duration: Theme.hoverMs
                easing.type: Easing.OutCubic
            }
        }
    }
}
