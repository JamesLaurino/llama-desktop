import QtQuick
import QtQuick.Layouts

ThemedDialog {
    id: control

    property string message: ""
    property string confirmText: qsTr("Supprimer")
    signal confirmed()

    width: 420

    contentItem: ColumnLayout {
        spacing: Theme.s4

        Text {
            Layout.fillWidth: true
            text: control.message
            color: Theme.text
            wrapMode: Text.WordWrap
            font.family: Theme.fontUi
            font.pixelSize: Theme.fontSize
            lineHeight: 1.3
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.s2

            Item {
                Layout.fillWidth: true
            }

            FlatButton {
                text: qsTr("Annuler")
                onClicked: control.close()
            }

            FlatButton {
                variant: "danger"
                text: control.confirmText
                onClicked: {
                    control.confirmed()
                    control.close()
                }
            }
        }
    }
}
