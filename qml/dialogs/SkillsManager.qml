import QtQuick
import UaApp

Item {
    id: root
    anchors.fill: parent
    signal closeRequested()

    property string filter: ""
    property var    allSkills: app.skills()
    property var    filteredSkills: {
        if (filter === "") return allSkills
        return allSkills.filter(s => s.dirName.toLowerCase().includes(filter.toLowerCase()))
    }

    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.35)
        MouseArea { anchors.fill: parent; onClicked: root.closeRequested() }
    }

    Rectangle {
        anchors.centerIn: parent
        width: 400
        height: 460
        radius: 10
        color: Palette.surface
        border.width: 1
        border.color: Palette.stroke
        MouseArea { anchors.fill: parent }  // block click-through

        TitleButton {
            anchors { top: parent.top; right: parent.right; margins: 6 }
            glyph: ""
            onClicked: root.closeRequested()
        }

        Text {
            id: title
            anchors { top: parent.top; left: parent.left; margins: 16 }
            text: Strings.t(Strings.SkillsTitle)
            color: Palette.text
            font.pixelSize: 14
            font.bold: true
        }
        Text {
            anchors { top: title.bottom; left: parent.left; margins: 16; topMargin: 2 }
            text: Strings.t(Strings.SkillCountFmt)
                      .arg(root.filteredSkills.length)
                      .arg(root.allSkills.length)
            color: Palette.sub
            font.pixelSize: 11
        }

        // Search
        Rectangle {
            id: searchBox
            anchors { top: parent.top; right: parent.right; margins: 16 }
            width: 160; height: 28; radius: 6
            color: Palette.surfaceAlt
            border.width: 1
            border.color: Palette.stroke

            TextInput {
                anchors { fill: parent; margins: 8 }
                clip: true
                color: Palette.text
                verticalAlignment: TextInput.AlignVCenter
                selectByMouse: true
                onTextChanged: root.filter = text
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    visible: parent.text === ""
                    text: Strings.t(Strings.SkillSearchPlaceholder)
                    color: Palette.sub
                    font.pixelSize: 11
                }
            }
        }

        ListView {
            id: skillList
            anchors {
                top: searchBox.bottom; left: parent.left; right: parent.right; bottom: footer.top
                margins: 16; topMargin: 10
            }
            clip: true
            spacing: 4
            model: root.filteredSkills

            delegate: Rectangle {
                width: skillList.width
                height: 32
                radius: 4
                color: hoverMa.containsMouse ? Palette.surfaceAlt : "transparent"

                Text {
                    anchors { left: parent.left; verticalCenter: parent.verticalCenter; leftMargin: 10 }
                    text: modelData.dirName
                    color: Palette.text
                    font.pixelSize: 12
                }
                Text {
                    anchors { right: removeBtn.left; verticalCenter: parent.verticalCenter; rightMargin: 10 }
                    text: Strings.t(Strings.SkillToolCountFmt).arg(modelData.installedCount)
                    color: Palette.sub
                    font.pixelSize: 10
                }
                Rectangle {
                    id: removeBtn
                    anchors { right: parent.right; verticalCenter: parent.verticalCenter; rightMargin: 6 }
                    width: 24; height: 24; radius: 4
                    color: rma.containsMouse ? (Palette.dark ? "#5A2A2A" : "#F6D5D5") : "transparent"
                    visible: modelData.installedCount > 0
                    IconGlyph {
                        anchors.centerIn: parent
                        text: ""
                        pixelSize: 12
                        color: rma.containsMouse ? (Palette.dark ? "#FF9A9A" : "#B33")
                                                 : Palette.sub
                    }
                    MouseArea {
                        id: rma
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            app.removeSkill(modelData.dirName)
                            root.allSkills = app.skills()
                        }
                    }
                }
                MouseArea {
                    id: hoverMa
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.NoButton
                }
            }
        }

        Text {
            anchors.centerIn: skillList
            visible: root.filteredSkills.length === 0
            text: Strings.t(Strings.SkillNoMatch)
            color: Palette.sub
            font.pixelSize: 12
        }

        Rectangle {
            id: footer
            anchors { bottom: parent.bottom; horizontalCenter: parent.horizontalCenter; bottomMargin: 16 }
            width: 200; height: 32; radius: 6
            color: fma.containsMouse ? Palette.accent : Qt.darker(Palette.accent, 1.1)

            IconGlyph {
                anchors { left: parent.left; verticalCenter: parent.verticalCenter; leftMargin: 10 }
                text: ""; pixelSize: 13; color: "#1A1A1A"
            }
            Text {
                anchors.centerIn: parent
                text: Strings.t(Strings.SkillsFolder)
                color: "#1A1A1A"
                font.pixelSize: 12
                font.bold: true
            }
            MouseArea {
                id: fma
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: app.openSkillsFolder()
            }
        }
    }
}
