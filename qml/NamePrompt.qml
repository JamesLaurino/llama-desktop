import QtQuick
import QtQuick.Layouts

ThemedDialog {
    id: control

    // `accepted` appartient déjà à Dialog : le signal porte un autre nom.
    signal submitted(string value)

    width: 420

    function openWith(initial) {
        field.text = initial
        open()
        field.forceActiveFocus()
        field.selectAll()
    }

    function commit() {
        const trimmed = field.text.trim()
        if (trimmed.length === 0)
            return
        control.submitted(trimmed)
        control.close()
    }

    contentItem: ColumnLayout {
        spacing: Theme.s4

        ThemedTextField {
            id: field
            Layout.fillWidth: true
            placeholderText: qsTr("Nom du profil")
            onAccepted: control.commit()
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
                variant: "primary"
                text: qsTr("Valider")
                enabled: field.text.trim().length > 0
                onClicked: control.commit()
            }
        }
    }
}
