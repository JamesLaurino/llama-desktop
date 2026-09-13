pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts

// Une ligne du formulaire : libellé, (?), contrôle adapté au type, marqueur
// d'état. Les propriétés requises sont remplies depuis les rôles du modèle.
Item {
    id: root

    required property string key
    required property string flag
    required property string flagOff
    required property string label
    required property string tooltip
    required property string type
    required property string defaultValue
    required property var minimum
    required property var maximum
    required property var values
    required property var keywords
    required property string value
    required property bool isSet
    required property bool isModified

    readonly property int intBottom: minimum !== undefined && minimum !== null
                                     ? Math.round(minimum) : -2147483647
    readonly property int intTop: maximum !== undefined && maximum !== null
                                  ? Math.round(maximum) : 2147483647
    readonly property string placeholder: defaultValue.length > 0
                                          ? qsTr("défaut : ") + defaultValue
                                          : qsTr("non défini")

    implicitHeight: Theme.controlHeight + Theme.s2
    width: parent ? parent.width : 0

    Rectangle {
        anchors.fill: parent
        anchors.leftMargin: -Theme.s2
        anchors.rightMargin: -Theme.s2
        radius: Theme.radiusControl
        color: rowHover.hovered ? Theme.hover : "transparent"

        Behavior on color {
            ColorAnimation {
                duration: Theme.hoverMs
                easing.type: Easing.OutCubic
            }
        }
    }

    HoverHandler {
        id: rowHover
    }

    RowLayout {
        anchors.fill: parent
        spacing: Theme.s3

        Text {
            Layout.preferredWidth: Theme.labelWidth
            Layout.maximumWidth: Theme.labelWidth
            text: root.label
            color: Theme.text
            font.family: Theme.fontUi
            font.pixelSize: Theme.fontSize
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }

        InfoTip {
            Layout.alignment: Qt.AlignVCenter
            flag: root.flagOff.length > 0 ? root.flag + " / " + root.flagOff : root.flag
            explanation: root.tooltip
            defaultValue: root.defaultValue
        }

        Loader {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            sourceComponent: {
                switch (root.type) {
                case "bool":
                    return boolControl
                case "tristate":
                    return tristateControl
                case "enum":
                    return enumControl
                case "int":
                    return intControl
                case "float":
                    return floatControl
                case "intOrKeyword":
                    return intOrKeywordControl
                case "path":
                    return pathControl
                default:
                    return stringControl
                }
            }
        }

        Rectangle {
            Layout.alignment: Qt.AlignVCenter
            visible: root.isModified
            implicitWidth: badge.implicitWidth + 2 * Theme.s2
            implicitHeight: 18
            radius: 9
            color: Theme.selected
            border.width: 1
            border.color: Theme.accentDim

            Text {
                id: badge
                anchors.centerIn: parent
                text: qsTr("modifié")
                color: Theme.accent
                font.family: Theme.fontUi
                font.pixelSize: Theme.fontSizeSmall
            }
        }

        Item {
            Layout.alignment: Qt.AlignVCenter
            implicitWidth: 8
            implicitHeight: 8
            visible: root.isSet && !root.isModified

            Rectangle {
                anchors.centerIn: parent
                width: 6
                height: 6
                radius: 3
                color: Theme.accent
            }

            HoverHandler {
                id: dotHover
            }

            ToolTip {
                visible: dotHover.hovered
                delay: Theme.tooltipDelay
                text: qsTr("Posé explicitement, à sa valeur par défaut : le flag est tout de même émis.")
            }
        }
    }

    Component {
        id: boolControl

        ThemedCheckBox {
            checked: root.value === "true"
            onToggled: App.setParamValue(root.key, checked ? "true" : "")
        }
    }

    Component {
        id: tristateControl

        Segmented {
            value: root.value
            options: [
                { label: qsTr("Défaut"), value: "" },
                { label: qsTr("Activé"), value: "on" },
                { label: qsTr("Désactivé"), value: "off" }
            ]
            onPicked: v => App.setParamValue(root.key, v)
        }
    }

    Component {
        id: enumControl

        ThemedComboBox {
            implicitWidth: 180
            model: [qsTr("(défaut)")].concat(root.values)
            currentIndex: {
                const i = root.values.indexOf(root.value)
                return i >= 0 ? i + 1 : 0
            }
            onActivated: i => App.setParamValue(root.key, i === 0 ? "" : root.values[i - 1])
        }
    }

    Component {
        id: intControl

        ThemedTextField {
            implicitWidth: 140
            text: root.value
            placeholderText: root.placeholder
            inputMethodHints: Qt.ImhDigitsOnly
            validator: IntValidator {
                bottom: root.intBottom
                top: root.intTop
            }
            onTextEdited: App.setParamValue(root.key, text)
        }
    }

    Component {
        id: floatControl

        ThemedTextField {
            implicitWidth: 140
            text: root.value
            placeholderText: root.placeholder
            validator: DoubleValidator {
                // Locale C : llama.cpp attend un point décimal, pas la virgule
                // française.
                locale: "C"
                notation: DoubleValidator.StandardNotation
                bottom: root.minimum !== undefined && root.minimum !== null ? root.minimum : -1e9
                top: root.maximum !== undefined && root.maximum !== null ? root.maximum : 1e9
                decimals: 4
            }
            onTextEdited: App.setParamValue(root.key, text)
        }
    }

    Component {
        id: intOrKeywordControl

        RowLayout {
            id: kwRow
            spacing: Theme.s2

            readonly property int valueModeIndex: root.keywords.length + 1

            ThemedComboBox {
                id: modeBox
                implicitWidth: 150
                model: [qsTr("(défaut)")].concat(root.keywords).concat([qsTr("valeur…")])
                currentIndex: {
                    if (root.value.length === 0)
                        return 0
                    const i = root.keywords.indexOf(root.value)
                    return i >= 0 ? i + 1 : kwRow.valueModeIndex
                }
                onActivated: i => {
                    if (i === 0)
                        App.setParamValue(root.key, "")
                    else if (i <= root.keywords.length)
                        App.setParamValue(root.key, root.keywords[i - 1])
                    else
                        App.setParamValue(root.key, String(root.intBottom > 0 ? root.intBottom : 0))
                }
            }

            ThemedTextField {
                Layout.preferredWidth: 110
                visible: modeBox.currentIndex === kwRow.valueModeIndex
                text: root.keywords.indexOf(root.value) >= 0 ? "" : root.value
                placeholderText: qsTr("entier")
                inputMethodHints: Qt.ImhDigitsOnly
                validator: IntValidator {
                    bottom: root.intBottom
                    top: root.intTop
                }
                onTextEdited: App.setParamValue(root.key, text)
            }

            Item {
                Layout.fillWidth: true
            }
        }
    }

    Component {
        id: pathControl

        RowLayout {
            spacing: Theme.s2

            ThemedTextField {
                id: pathField
                Layout.fillWidth: true
                mono: true
                text: root.value
                placeholderText: root.placeholder
                onTextEdited: App.setParamValue(root.key, text)
            }

            FlatButton {
                text: qsTr("Parcourir")
                onClicked: pathDialog.open()
            }

            FileDialog {
                id: pathDialog
                title: root.label
                currentFolder: App.fileUrl(App.defaultModelsDir)
                onAccepted: App.setParamValue(root.key, App.localFile(selectedFile))
            }
        }
    }

    Component {
        id: stringControl

        ThemedTextField {
            text: root.value
            placeholderText: root.placeholder
            onTextEdited: App.setParamValue(root.key, text)
        }
    }
}
