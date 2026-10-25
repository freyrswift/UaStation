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
        color: Theme.surface
        border.width: 1
        border.color: Theme.stroke
        MouseArea { anchors.fill: parent }  // block click-through

        TitleButton {
            anchors { top: parent.top; right: parent.right; margins: 6 }
            glyph: ""
            onClicked: root.closeRequested()
        }

        Text {
            id: title
            anchors { top: parent.top; left: parent.left; margins: 16 }
            text: (Strings.lang, Strings.t(Strings.SkillsTitle))
            color: Theme.text
            font.pixelSize: 14
            font.bold: true
        }
        Text {
            anchors { top: title.bottom; left: parent.left; margins: 16; topMargin: 2 }
            text: (Strings.lang, Strings.t(Strings.SkillCountFmt))
                      .arg(root.filteredSkills.length)
                      .arg(root.allSkills.length)
            color: Theme.sub
            font.pixelSize: 11
        }

        // Search
        Rectangle {
            id: searchBox
            anchors { top: parent.top; right: parent.right; margins: 16 }
            width: 160; height: 28; radius: 6
            color: Theme.surfaceAlt
            border.width: 1
            border.color: Theme.stroke

            TextInput {
                anchors { fill: parent; margins: 8 }
                clip: true
                color: Theme.text
                verticalAlignment: TextInput.AlignVCenter
                selectByMouse: true
                onTextChanged: root.filter = text
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    visible: parent.text === ""
                    text: (Strings.lang, Strings.t(Strings.SkillSearchPlaceholder))
                    color: Theme.sub
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
                color: hoverMa.containsMouse ? Theme.surfaceAlt : "transparent"

                Text {
                    anchors { left: parent.left; verticalCenter: parent.verticalCenter; leftMargin: 10 }
                    text: modelData.dirName
                    color: Theme.text
                    font.pixelSize: 12
                }
                Text {
                    anchors { right: removeBtn.left; verticalCenter: parent.verticalCenter; rightMargin: 10 }
                    text: (Strings.lang, Strings.t(Strings.SkillToolCountFmt)).arg(modelData.installedCount)
                    color: Theme.sub
                    font.pixelSize: 10
                }
                Rectangle {
                    id: removeBtn
                    anchors { right: parent.right; verticalCenter: parent.verticalCenter; rightMargin: 6 }
                    width: 24; height: 24; radius: 4
                    color: rma.containsMouse ? (Theme.dark ? "#5A2A2A" : "#F6D5D5") : "transparent"
                    visible: modelData.installedCount > 0
                    IconGlyph {
                        anchors.centerIn: parent
                        text: ""
                        pixelSize: 12
                        color: rma.containsMouse ? (Theme.dark ? "#FF9A9A" : "#B33")
                                                 : Theme.sub
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
            text: (Strings.lang, Strings.t(Strings.SkillNoMatch))
            color: Theme.sub
            font.pixelSize: 12
        }

        Rectangle {
            id: footer
            anchors { bottom: parent.bottom; horizontalCenter: parent.horizontalCenter; bottomMargin: 16 }
            width: 200; height: 32; radius: 6
            color: fma.containsMouse ? Theme.accent : Qt.darker(Theme.accent, 1.1)

            IconGlyph {
                anchors { left: parent.left; verticalCenter: parent.verticalCenter; leftMargin: 10 }
                text: ""; pixelSize: 13; color: "#1A1A1A"
            }
            Text {
                anchors.centerIn: parent
                text: (Strings.lang, Strings.t(Strings.SkillsFolder))
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
