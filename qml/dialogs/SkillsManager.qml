import QtQuick
import UaApp

Item {
    id: root
    anchors.fill: parent
    signal closeRequested()

    property string filter: ""
    property int    activeCategory: 0   // 0 = All, 1..9 = SkillCategory enum values
    property var    allSkills: app.skills()
    property var    filteredSkills: {
        const q = filter.toLowerCase()
        return allSkills.filter(s => {
            if (q !== "" && !s.dirName.toLowerCase().includes(q)) return false
            if (activeCategory !== 0 && s.category !== activeCategory) return false
            return true
        })
    }

    // Category chip labels, in SkillCategory enum order
    readonly property var categoryLabels: [
        Strings.t(Strings.CatAll),
        Strings.t(Strings.CatReverse),
        Strings.t(Strings.CatGame),
        Strings.t(Strings.CatKernel),
        Strings.t(Strings.CatExploit),
        Strings.t(Strings.CatCrack),
        Strings.t(Strings.CatMobile),
        Strings.t(Strings.CatPentest),
        Strings.t(Strings.CatAi),
        Strings.t(Strings.CatOther),
    ]

    // Dim backdrop
    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.35)
        MouseArea { anchors.fill: parent; onClicked: root.closeRequested() }
    }

    // Dialog panel
    Rectangle {
        id: panel
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 80
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 40
        width: 360
        radius: 12
        color: Theme.surface
        border.width: 1
        border.color: Theme.stroke
        MouseArea { anchors.fill: parent }  // block click-through

        readonly property int pad: 20

        // Header: title + count on the left, close × on the right (all equally padded)
        Item {
            id: header
            anchors { top: parent.top; left: parent.left; right: parent.right
                      leftMargin: panel.pad; rightMargin: panel.pad; topMargin: panel.pad }
            height: 36

            Text {
                id: title
                anchors { left: parent.left; top: parent.top }
                text: (Strings.lang, Strings.t(Strings.SkillsTitle))
                color: Theme.text
                font.pixelSize: 15
                font.bold: true
            }
            Text {
                anchors { left: parent.left; top: title.bottom; topMargin: 2 }
                text: (Strings.lang, Strings.t(Strings.SkillCountFmt))
                          .arg(root.filteredSkills.length)
                          .arg(root.allSkills.length)
                color: Theme.sub
                font.pixelSize: 11
            }
            TitleButton {
                anchors { right: parent.right; verticalCenter: title.verticalCenter }
                glyph: "\uE6D3"
                onClicked: root.closeRequested()
            }
        }

        // Search box with leading magnifier glyph
        Rectangle {
            id: searchBox
            anchors { top: header.bottom; left: parent.left; right: parent.right
                      leftMargin: panel.pad; rightMargin: panel.pad; topMargin: 14 }
            height: 32
            radius: 6
            color: Theme.dark ? Qt.rgba(1,1,1,0.05) : Qt.rgba(0,0,0,0.03)
            border.width: 1
            border.color: searchInput.activeFocus ? Theme.accent : Theme.stroke

            IconGlyph {
                id: searchIcon
                anchors { left: parent.left; verticalCenter: parent.verticalCenter; leftMargin: 10 }
                text: "\uEFD7"  // U+efd7 Search
                pixelSize: 13
                color: Theme.sub
            }
            TextInput {
                id: searchInput
                anchors { left: searchIcon.right; right: parent.right
                          verticalCenter: parent.verticalCenter
                          leftMargin: 8; rightMargin: 10 }
                clip: true
                color: Theme.text
                font.pixelSize: 12
                selectByMouse: true
                onTextChanged: root.filter = text
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    visible: searchInput.text === ""
                    text: (Strings.lang, Strings.t(Strings.SkillSearchPlaceholder))
                    color: Theme.sub
                    font.pixelSize: 12
                }
            }
        }

        // Category chip row (horizontally scrollable)
        Flickable {
            id: chipRow
            anchors { top: searchBox.bottom; left: parent.left; right: parent.right
                      leftMargin: panel.pad; rightMargin: panel.pad; topMargin: 10 }
            height: 26
            clip: true
            contentWidth: chipLayout.width
            boundsBehavior: Flickable.StopAtBounds

            Row {
                id: chipLayout
                spacing: 6
                Repeater {
                    model: root.categoryLabels
                    delegate: Rectangle {
                        height: 24
                        width: chipLabel.implicitWidth + 20
                        radius: 12
                        color: index === root.activeCategory
                               ? Theme.accent
                               : (chipMa.containsMouse
                                  ? (Theme.dark ? Qt.rgba(1,1,1,0.08) : Qt.rgba(0,0,0,0.04))
                                  : "transparent")
                        border.width: 1
                        border.color: index === root.activeCategory ? Theme.accent : Theme.stroke
                        Text {
                            id: chipLabel
                            anchors.centerIn: parent
                            text: modelData
                            color: index === root.activeCategory ? "#1A1A1A" : Theme.text
                            font.pixelSize: 11
                            font.bold: index === root.activeCategory
                        }
                        MouseArea {
                            id: chipMa
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.activeCategory = index
                        }
                    }
                }
            }
        }

        // Separator between chips and list
        Rectangle {
            id: separator
            anchors { top: chipRow.bottom; left: parent.left; right: parent.right
                      leftMargin: panel.pad; rightMargin: panel.pad; topMargin: 10 }
            height: 1
            color: Theme.stroke
        }

        // Skill list
        ListView {
            id: skillList
            anchors {
                top: separator.bottom; left: parent.left; right: parent.right; bottom: footer.top
                leftMargin: panel.pad - 4; rightMargin: panel.pad - 4
                topMargin: 6; bottomMargin: 6
            }
            clip: true
            spacing: 2
            model: root.filteredSkills

            delegate: Rectangle {
                width: skillList.width
                height: 34
                radius: 6
                color: hoverMa.containsMouse ? (Theme.dark ? Qt.rgba(1,1,1,0.04)
                                                           : Qt.rgba(0,0,0,0.02))
                                             : "transparent"

                // Left: name
                Text {
                    anchors { left: parent.left; verticalCenter: parent.verticalCenter; leftMargin: 10 }
                    text: modelData.dirName
                    color: Theme.text
                    font.pixelSize: 12
                }
                // Right: count badge + × delete, equidistant from edge
                Row {
                    anchors { right: parent.right; verticalCenter: parent.verticalCenter; rightMargin: 6 }
                    spacing: 4

                    // Small badge showing N tools
                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        width: countLabel.implicitWidth + 14
                        height: 20
                        radius: 10
                        color: modelData.installedCount > 0
                               ? (Theme.dark ? Qt.rgba(80/255,195/255,89/255,0.18)
                                             : Qt.rgba(46/255,189/255,89/255,0.14))
                               : (Theme.dark ? Qt.rgba(1,1,1,0.05) : Qt.rgba(0,0,0,0.04))
                        Text {
                            id: countLabel
                            anchors.centerIn: parent
                            text: (Strings.lang, Strings.t(Strings.SkillToolCountFmt))
                                      .arg(modelData.installedCount)
                            color: modelData.installedCount > 0 ? Theme.green : Theme.sub
                            font.pixelSize: 10
                            font.bold: modelData.installedCount > 0
                        }
                    }

                    // × remove (only when something to remove)
                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 24; height: 24; radius: 4
                        color: rma.containsMouse ? (Theme.dark ? "#5A2A2A" : "#F6D5D5")
                                                 : "transparent"
                        visible: modelData.installedCount > 0
                        IconGlyph {
                            anchors.centerIn: parent
                            text: "\uE6D3"
                            pixelSize: 11
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
                }
                MouseArea {
                    id: hoverMa
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.NoButton
                }
            }
        }

        // Empty-list message
        Text {
            anchors.centerIn: skillList
            visible: root.filteredSkills.length === 0
            text: (Strings.lang, Strings.t(Strings.SkillNoMatch))
            color: Theme.sub
            font.pixelSize: 12
        }

        // Footer: full-width primary action with icon on the left + label centered
        Rectangle {
            id: footer
            anchors { bottom: parent.bottom; left: parent.left; right: parent.right
                      leftMargin: panel.pad; rightMargin: panel.pad; bottomMargin: panel.pad }
            height: 36
            radius: 8
            color: fma.containsMouse ? Qt.lighter(Theme.accent, 1.05) : Theme.accent

            Row {
                anchors.centerIn: parent
                spacing: 8
                IconGlyph {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "\uE8EB"  // U+e8eb FolderOpen
                    pixelSize: 14
                    color: "#1A1A1A"
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: (Strings.lang, Strings.t(Strings.SkillsFolder))
                    color: "#1A1A1A"
                    font.pixelSize: 13
                    font.bold: true
                }
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
