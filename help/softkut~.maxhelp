{
    "patcher": {
        "fileversion": 1,
        "appversion": {
            "major": 8,
            "minor": 5,
            "revision": 5,
            "architecture": "x64",
            "modernui": 1
        },
        "classnamespace": "box",
        "rect": [
            100.0,
            100.0,
            740.0,
            760.0
        ],
        "bglocked": 0,
        "openinpresentation": 0,
        "default_fontsize": 12.0,
        "default_fontface": 0,
        "default_fontname": "Arial",
        "gridonopen": 1,
        "gridsize": [
            15.0,
            15.0
        ],
        "gridsnaponopen": 1,
        "objectsnaponopen": 1,
        "statusbarvisible": 2,
        "toolbarvisible": 1,
        "lefttoolbarpinned": 0,
        "toptoolbarpinned": 0,
        "righttoolbarpinned": 0,
        "bottomtoolbarpinned": 0,
        "toolbars_unpinned_last_save": 0,
        "tallnewobj": 0,
        "boxanimatetime": 200,
        "enablehscroll": 1,
        "enablevscroll": 1,
        "devicewidth": 0.0,
        "description": "",
        "digest": "",
        "tags": "",
        "style": "",
        "subpatcher_template": "",
        "assistshowspatchername": 0,
        "boxes": [
            {
                "box": {
                    "id": "obj-1",
                    "maxclass": "newobj",
                    "numinlets": 0,
                    "numoutlets": 0,
                    "patching_rect": [
                        15.0,
                        60.0,
                        100.0,
                        22.0
                    ],
                    "patcher": {
                        "fileversion": 1,
                        "appversion": {
                            "major": 8,
                            "minor": 5,
                            "revision": 5,
                            "architecture": "x64",
                            "modernui": 1
                        },
                        "classnamespace": "box",
                        "rect": [
                            0.0,
                            26.0,
                            720.0,
                            680.0
                        ],
                        "bglocked": 0,
                        "openinpresentation": 0,
                        "default_fontsize": 12.0,
                        "default_fontface": 0,
                        "default_fontname": "Arial",
                        "gridonopen": 1,
                        "gridsize": [
                            15.0,
                            15.0
                        ],
                        "gridsnaponopen": 1,
                        "objectsnaponopen": 1,
                        "statusbarvisible": 2,
                        "toolbarvisible": 1,
                        "lefttoolbarpinned": 0,
                        "toptoolbarpinned": 0,
                        "righttoolbarpinned": 0,
                        "bottomtoolbarpinned": 0,
                        "toolbars_unpinned_last_save": 0,
                        "tallnewobj": 0,
                        "boxanimatetime": 200,
                        "enablehscroll": 1,
                        "enablevscroll": 1,
                        "devicewidth": 0.0,
                        "description": "",
                        "digest": "",
                        "tags": "",
                        "style": "",
                        "subpatcher_template": "",
                        "assistshowspatchername": 0,
                        "boxes": [
                            {
                                "box": {
                                    "id": "obj-1",
                                    "maxclass": "comment",
                                    "numinlets": 1,
                                    "numoutlets": 0,
                                    "patching_rect": [
                                        15.0,
                                        12.0,
                                        640.0,
                                        28.0
                                    ],
                                    "text": "Loop any buffer~",
                                    "fontsize": 16.0
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-2",
                                    "maxclass": "comment",
                                    "numinlets": 1,
                                    "numoutlets": 0,
                                    "patching_rect": [
                                        15.0,
                                        48.0,
                                        660.0,
                                        36.0
                                    ],
                                    "text": "Messages take <voice> <value>; voices count from 0. Times are seconds of buffer material. The view at the bottom shows the buffer~, the moving play head and the loop window."
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-3",
                                    "maxclass": "message",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "patching_rect": [
                                        15.0,
                                        98.0,
                                        296.0,
                                        22.0
                                    ],
                                    "text": "loopstart 0 0.2, loopend 0 1.2, play 0 1",
                                    "outlettype": [
                                        ""
                                    ]
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-4",
                                    "maxclass": "message",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "patching_rect": [
                                        15.0,
                                        128.0,
                                        72.0,
                                        22.0
                                    ],
                                    "text": "play 0 0",
                                    "outlettype": [
                                        ""
                                    ]
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-5",
                                    "maxclass": "comment",
                                    "numinlets": 1,
                                    "numoutlets": 0,
                                    "patching_rect": [
                                        95.0,
                                        128.0,
                                        330.0,
                                        36.0
                                    ],
                                    "text": "play starts inside the loop: no position message needed"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-6",
                                    "maxclass": "message",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "patching_rect": [
                                        15.0,
                                        172.0,
                                        72.0,
                                        22.0
                                    ],
                                    "text": "rate 0 1",
                                    "outlettype": [
                                        ""
                                    ]
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-7",
                                    "maxclass": "message",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "patching_rect": [
                                        95.0,
                                        172.0,
                                        86.0,
                                        22.0
                                    ],
                                    "text": "rate 0 0.5",
                                    "outlettype": [
                                        ""
                                    ]
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-8",
                                    "maxclass": "message",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "patching_rect": [
                                        189.0,
                                        172.0,
                                        79.0,
                                        22.0
                                    ],
                                    "text": "rate 0 -1",
                                    "outlettype": [
                                        ""
                                    ]
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-9",
                                    "maxclass": "comment",
                                    "numinlets": 1,
                                    "numoutlets": 0,
                                    "patching_rect": [
                                        276.0,
                                        172.0,
                                        200.0,
                                        36.0
                                    ],
                                    "text": "0.5 = octave down, -1 = reverse"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-10",
                                    "maxclass": "message",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "patching_rect": [
                                        15.0,
                                        216.0,
                                        114.0,
                                        22.0
                                    ],
                                    "text": "position 0 0.6",
                                    "outlettype": [
                                        ""
                                    ]
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-11",
                                    "maxclass": "comment",
                                    "numinlets": 1,
                                    "numoutlets": 0,
                                    "patching_rect": [
                                        137.0,
                                        216.0,
                                        220.0,
                                        21.0
                                    ],
                                    "text": "jump the play head (seconds)"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-12",
                                    "maxclass": "message",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "patching_rect": [
                                        15.0,
                                        246.0,
                                        51.0,
                                        22.0
                                    ],
                                    "text": "reset",
                                    "outlettype": [
                                        ""
                                    ]
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-13",
                                    "maxclass": "comment",
                                    "numinlets": 1,
                                    "numoutlets": 0,
                                    "patching_rect": [
                                        74.0,
                                        246.0,
                                        360.0,
                                        51.0
                                    ],
                                    "text": "reset: stop every voice and restore every default (loop 0-1 s, rate 1, level 1, no feedback). The buffer~ and @report are kept."
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-14",
                                    "maxclass": "newobj",
                                    "numinlets": 1,
                                    "numoutlets": 1,
                                    "patching_rect": [
                                        480.0,
                                        98.0,
                                        72.0,
                                        22.0
                                    ],
                                    "text": "loadbang",
                                    "outlettype": [
                                        "bang"
                                    ]
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-15",
                                    "maxclass": "message",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "patching_rect": [
                                        480.0,
                                        128.0,
                                        156.0,
                                        22.0
                                    ],
                                    "text": "replace cello-f2.aif",
                                    "outlettype": [
                                        ""
                                    ]
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-16",
                                    "maxclass": "newobj",
                                    "numinlets": 1,
                                    "numoutlets": 2,
                                    "patching_rect": [
                                        480.0,
                                        158.0,
                                        135.0,
                                        22.0
                                    ],
                                    "text": "buffer~ skh_basic",
                                    "outlettype": [
                                        "float",
                                        "bang"
                                    ]
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-17",
                                    "maxclass": "comment",
                                    "numinlets": 1,
                                    "numoutlets": 0,
                                    "patching_rect": [
                                        480.0,
                                        188.0,
                                        220.0,
                                        66.0
                                    ],
                                    "text": "replace sizes the buffer~ to the file. Every frame is used; the length need not be a power of two."
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-18",
                                    "maxclass": "newobj",
                                    "numinlets": 1,
                                    "numoutlets": 2,
                                    "patching_rect": [
                                        15.0,
                                        313.0,
                                        142.0,
                                        22.0
                                    ],
                                    "text": "softkut~ skh_basic",
                                    "outlettype": [
                                        "signal",
                                        ""
                                    ]
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-19",
                                    "maxclass": "newobj",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "patching_rect": [
                                        480.0,
                                        313.0,
                                        58.0,
                                        22.0
                                    ],
                                    "text": "*~ 0.5",
                                    "outlettype": [
                                        "signal"
                                    ]
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-20",
                                    "maxclass": "ezdac~",
                                    "numinlets": 2,
                                    "numoutlets": 0,
                                    "patching_rect": [
                                        546.0,
                                        313.0,
                                        45.0,
                                        45.0
                                    ],
                                    "parameter_enable": 0
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-21",
                                    "maxclass": "comment",
                                    "numinlets": 1,
                                    "numoutlets": 0,
                                    "patching_rect": [
                                        599.0,
                                        313.0,
                                        90.0,
                                        21.0
                                    ],
                                    "text": "audio on/off"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-22",
                                    "maxclass": "bpatcher",
                                    "numinlets": 1,
                                    "numoutlets": 1,
                                    "patching_rect": [
                                        15.0,
                                        366.0,
                                        520.0,
                                        180.0
                                    ],
                                    "name": "softkut.view.maxpat",
                                    "args": [
                                        "skh_basic",
                                        1,
                                        0
                                    ],
                                    "outlettype": [
                                        ""
                                    ],
                                    "offset": [
                                        0.0,
                                        0.0
                                    ],
                                    "viewvisibility": 1,
                                    "bgmode": 0,
                                    "border": 0,
                                    "clickthrough": 0,
                                    "enablehscroll": 0,
                                    "enablevscroll": 0,
                                    "lockeddragscroll": 0
                                }
                            }
                        ],
                        "lines": [
                            {
                                "patchline": {
                                    "source": [
                                        "obj-14",
                                        0
                                    ],
                                    "destination": [
                                        "obj-15",
                                        0
                                    ],
                                    "order": 0
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-15",
                                        0
                                    ],
                                    "destination": [
                                        "obj-16",
                                        0
                                    ],
                                    "order": 0
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-18",
                                        0
                                    ],
                                    "destination": [
                                        "obj-19",
                                        0
                                    ],
                                    "order": 0
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-19",
                                        0
                                    ],
                                    "destination": [
                                        "obj-20",
                                        0
                                    ],
                                    "order": 0
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-19",
                                        0
                                    ],
                                    "destination": [
                                        "obj-20",
                                        1
                                    ],
                                    "order": 1
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-3",
                                        0
                                    ],
                                    "destination": [
                                        "obj-18",
                                        0
                                    ],
                                    "order": 0
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-4",
                                        0
                                    ],
                                    "destination": [
                                        "obj-18",
                                        0
                                    ],
                                    "order": 0
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-6",
                                        0
                                    ],
                                    "destination": [
                                        "obj-18",
                                        0
                                    ],
                                    "order": 0
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-7",
                                        0
                                    ],
                                    "destination": [
                                        "obj-18",
                                        0
                                    ],
                                    "order": 0
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-8",
                                        0
                                    ],
                                    "destination": [
                                        "obj-18",
                                        0
                                    ],
                                    "order": 0
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-10",
                                        0
                                    ],
                                    "destination": [
                                        "obj-18",
                                        0
                                    ],
                                    "order": 0
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-12",
                                        0
                                    ],
                                    "destination": [
                                        "obj-18",
                                        0
                                    ],
                                    "order": 0
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-18",
                                        1
                                    ],
                                    "destination": [
                                        "obj-22",
                                        0
                                    ],
                                    "order": 0
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-22",
                                        0
                                    ],
                                    "destination": [
                                        "obj-18",
                                        0
                                    ],
                                    "order": 0
                                }
                            }
                        ],
                        "dependency_cache": [],
                        "autosave": 0,
                        "showontab": 1
                    },
                    "text": "p basic",
                    "outlettype": [
                        ""
                    ]
                }
            },
            {
                "box": {
                    "id": "obj-2",
                    "maxclass": "newobj",
                    "numinlets": 0,
                    "numoutlets": 0,
                    "patching_rect": [
                        125.0,
                        60.0,
                        100.0,
                        22.0
                    ],
                    "patcher": {
                        "fileversion": 1,
                        "appversion": {
                            "major": 8,
                            "minor": 5,
                            "revision": 5,
                            "architecture": "x64",
                            "modernui": 1
                        },
                        "classnamespace": "box",
                        "rect": [
                            0.0,
                            26.0,
                            720.0,
                            877.0
                        ],
                        "bglocked": 0,
                        "openinpresentation": 0,
                        "default_fontsize": 12.0,
                        "default_fontface": 0,
                        "default_fontname": "Arial",
                        "gridonopen": 1,
                        "gridsize": [
                            15.0,
                            15.0
                        ],
                        "gridsnaponopen": 1,
                        "objectsnaponopen": 1,
                        "statusbarvisible": 2,
                        "toolbarvisible": 1,
                        "lefttoolbarpinned": 0,
                        "toptoolbarpinned": 0,
                        "righttoolbarpinned": 0,
                        "bottomtoolbarpinned": 0,
                        "toolbars_unpinned_last_save": 0,
                        "tallnewobj": 0,
                        "boxanimatetime": 200,
                        "enablehscroll": 1,
                        "enablevscroll": 1,
                        "devicewidth": 0.0,
                        "description": "",
                        "digest": "",
                        "tags": "",
                        "style": "",
                        "subpatcher_template": "",
                        "assistshowspatchername": 0,
                        "boxes": [
                            {
                                "box": {
                                    "id": "obj-1",
                                    "maxclass": "comment",
                                    "numinlets": 1,
                                    "numoutlets": 0,
                                    "patching_rect": [
                                        15.0,
                                        12.0,
                                        640.0,
                                        28.0
                                    ],
                                    "text": "Multichannel buffer~",
                                    "fontsize": 16.0
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-2",
                                    "maxclass": "comment",
                                    "numinlets": 1,
                                    "numoutlets": 0,
                                    "patching_rect": [
                                        15.0,
                                        48.0,
                                        660.0,
                                        36.0
                                    ],
                                    "text": "Each voice reads and writes one channel. By default voice v uses channel v mod the channel count: on this stereo buffer~, voice 0 plays the left channel (bells) and voice 1 the right (bass)."
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-3",
                                    "maxclass": "message",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "patching_rect": [
                                        15.0,
                                        98.0,
                                        324.0,
                                        22.0
                                    ],
                                    "text": "loopend 0 2, loopend 1 2, play 0 1, play 1 1",
                                    "outlettype": [
                                        ""
                                    ]
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-4",
                                    "maxclass": "message",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "patching_rect": [
                                        15.0,
                                        128.0,
                                        142.0,
                                        22.0
                                    ],
                                    "text": "play 0 0, play 1 0",
                                    "outlettype": [
                                        ""
                                    ]
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-5",
                                    "maxclass": "message",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "patching_rect": [
                                        15.0,
                                        158.0,
                                        114.0,
                                        22.0
                                    ],
                                    "text": "set skh_stereo",
                                    "outlettype": [
                                        ""
                                    ]
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-6",
                                    "maxclass": "comment",
                                    "numinlets": 1,
                                    "numoutlets": 0,
                                    "patching_rect": [
                                        137.0,
                                        158.0,
                                        300.0,
                                        36.0
                                    ],
                                    "text": "default: voice 0 -> channel 1, voice 1 -> channel 2"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-7",
                                    "maxclass": "message",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "patching_rect": [
                                        15.0,
                                        202.0,
                                        128.0,
                                        22.0
                                    ],
                                    "text": "set skh_stereo 1",
                                    "outlettype": [
                                        ""
                                    ]
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-8",
                                    "maxclass": "comment",
                                    "numinlets": 1,
                                    "numoutlets": 0,
                                    "patching_rect": [
                                        151.0,
                                        202.0,
                                        300.0,
                                        36.0
                                    ],
                                    "text": "both voices read channel 1 (channels count from 1)"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-9",
                                    "maxclass": "message",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "patching_rect": [
                                        15.0,
                                        246.0,
                                        177.0,
                                        22.0
                                    ],
                                    "text": "voicebuf 1 skh_stereo 2",
                                    "outlettype": [
                                        ""
                                    ]
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-10",
                                    "maxclass": "comment",
                                    "numinlets": 1,
                                    "numoutlets": 0,
                                    "patching_rect": [
                                        200.0,
                                        246.0,
                                        220.0,
                                        21.0
                                    ],
                                    "text": "voice 1 alone reads channel 2"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-11",
                                    "maxclass": "message",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "patching_rect": [
                                        15.0,
                                        276.0,
                                        177.0,
                                        22.0
                                    ],
                                    "text": "voicebuf 1 skh_stereo 3",
                                    "outlettype": [
                                        ""
                                    ]
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-12",
                                    "maxclass": "comment",
                                    "numinlets": 1,
                                    "numoutlets": 0,
                                    "patching_rect": [
                                        200.0,
                                        276.0,
                                        230.0,
                                        36.0
                                    ],
                                    "text": "no channel 3: voice 1 falls silent, with one console warning"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-13",
                                    "maxclass": "message",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "patching_rect": [
                                        15.0,
                                        320.0,
                                        86.0,
                                        22.0
                                    ],
                                    "text": "rate 1 0.5",
                                    "outlettype": [
                                        ""
                                    ]
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-14",
                                    "maxclass": "comment",
                                    "numinlets": 1,
                                    "numoutlets": 0,
                                    "patching_rect": [
                                        109.0,
                                        320.0,
                                        150.0,
                                        21.0
                                    ],
                                    "text": "bass an octave down"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-15",
                                    "maxclass": "message",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "patching_rect": [
                                        15.0,
                                        350.0,
                                        51.0,
                                        22.0
                                    ],
                                    "text": "reset",
                                    "outlettype": [
                                        ""
                                    ]
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-16",
                                    "maxclass": "comment",
                                    "numinlets": 1,
                                    "numoutlets": 0,
                                    "patching_rect": [
                                        74.0,
                                        350.0,
                                        100.0,
                                        21.0
                                    ],
                                    "text": "start over"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-17",
                                    "maxclass": "newobj",
                                    "numinlets": 1,
                                    "numoutlets": 1,
                                    "patching_rect": [
                                        480.0,
                                        98.0,
                                        72.0,
                                        22.0
                                    ],
                                    "text": "loadbang",
                                    "outlettype": [
                                        "bang"
                                    ]
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-18",
                                    "maxclass": "message",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "patching_rect": [
                                        480.0,
                                        128.0,
                                        198.0,
                                        22.0
                                    ],
                                    "text": "replace softkut-stereo.wav",
                                    "outlettype": [
                                        ""
                                    ]
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-19",
                                    "maxclass": "newobj",
                                    "numinlets": 1,
                                    "numoutlets": 2,
                                    "patching_rect": [
                                        480.0,
                                        158.0,
                                        142.0,
                                        22.0
                                    ],
                                    "text": "buffer~ skh_stereo",
                                    "outlettype": [
                                        "float",
                                        "bang"
                                    ]
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-20",
                                    "maxclass": "comment",
                                    "numinlets": 1,
                                    "numoutlets": 0,
                                    "patching_rect": [
                                        480.0,
                                        188.0,
                                        220.0,
                                        66.0
                                    ],
                                    "text": "softkut-stereo.wav is in the package's media folder. If Max reports it cannot open the file, restart Max so it finds the folder."
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-21",
                                    "maxclass": "newobj",
                                    "numinlets": 2,
                                    "numoutlets": 3,
                                    "patching_rect": [
                                        15.0,
                                        388.0,
                                        163.0,
                                        22.0
                                    ],
                                    "text": "softkut~ skh_stereo 2",
                                    "outlettype": [
                                        "signal",
                                        "signal",
                                        ""
                                    ]
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-22",
                                    "maxclass": "newobj",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "patching_rect": [
                                        480.0,
                                        388.0,
                                        58.0,
                                        22.0
                                    ],
                                    "text": "*~ 0.5",
                                    "outlettype": [
                                        "signal"
                                    ]
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-23",
                                    "maxclass": "newobj",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "patching_rect": [
                                        546.0,
                                        388.0,
                                        58.0,
                                        22.0
                                    ],
                                    "text": "*~ 0.5",
                                    "outlettype": [
                                        "signal"
                                    ]
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-24",
                                    "maxclass": "ezdac~",
                                    "numinlets": 2,
                                    "numoutlets": 0,
                                    "patching_rect": [
                                        612.0,
                                        388.0,
                                        45.0,
                                        45.0
                                    ],
                                    "parameter_enable": 0
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-25",
                                    "maxclass": "comment",
                                    "numinlets": 1,
                                    "numoutlets": 0,
                                    "patching_rect": [
                                        665.0,
                                        388.0,
                                        90.0,
                                        21.0
                                    ],
                                    "text": "audio on/off"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-26",
                                    "maxclass": "bpatcher",
                                    "numinlets": 1,
                                    "numoutlets": 1,
                                    "patching_rect": [
                                        15.0,
                                        441.0,
                                        520.0,
                                        180.0
                                    ],
                                    "name": "softkut.view.maxpat",
                                    "args": [
                                        "skh_stereo",
                                        1,
                                        0
                                    ],
                                    "outlettype": [
                                        ""
                                    ],
                                    "offset": [
                                        0.0,
                                        0.0
                                    ],
                                    "viewvisibility": 1,
                                    "bgmode": 0,
                                    "border": 0,
                                    "clickthrough": 0,
                                    "enablehscroll": 0,
                                    "enablevscroll": 0,
                                    "lockeddragscroll": 0
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-27",
                                    "maxclass": "bpatcher",
                                    "numinlets": 1,
                                    "numoutlets": 1,
                                    "patching_rect": [
                                        15.0,
                                        629.0,
                                        520.0,
                                        180.0
                                    ],
                                    "name": "softkut.view.maxpat",
                                    "args": [
                                        "skh_stereo",
                                        2,
                                        1
                                    ],
                                    "outlettype": [
                                        ""
                                    ],
                                    "offset": [
                                        0.0,
                                        0.0
                                    ],
                                    "viewvisibility": 1,
                                    "bgmode": 0,
                                    "border": 0,
                                    "clickthrough": 0,
                                    "enablehscroll": 0,
                                    "enablevscroll": 0,
                                    "lockeddragscroll": 0
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-28",
                                    "maxclass": "comment",
                                    "numinlets": 1,
                                    "numoutlets": 0,
                                    "patching_rect": [
                                        15.0,
                                        817.0,
                                        520.0,
                                        36.0
                                    ],
                                    "text": "The views show channels 1 and 2. After set skh_stereo 1, voice 1's play head still moves over the channel 2 view, but it reads channel 1."
                                }
                            }
                        ],
                        "lines": [
                            {
                                "patchline": {
                                    "source": [
                                        "obj-17",
                                        0
                                    ],
                                    "destination": [
                                        "obj-18",
                                        0
                                    ],
                                    "order": 0
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-18",
                                        0
                                    ],
                                    "destination": [
                                        "obj-19",
                                        0
                                    ],
                                    "order": 0
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-21",
                                        0
                                    ],
                                    "destination": [
                                        "obj-22",
                                        0
                                    ],
                                    "order": 0
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-22",
                                        0
                                    ],
                                    "destination": [
                                        "obj-24",
                                        0
                                    ],
                                    "order": 0
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-21",
                                        1
                                    ],
                                    "destination": [
                                        "obj-23",
                                        0
                                    ],
                                    "order": 0
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-23",
                                        0
                                    ],
                                    "destination": [
                                        "obj-24",
                                        1
                                    ],
                                    "order": 0
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-3",
                                        0
                                    ],
                                    "destination": [
                                        "obj-21",
                                        0
                                    ],
                                    "order": 0
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-4",
                                        0
                                    ],
                                    "destination": [
                                        "obj-21",
                                        0
                                    ],
                                    "order": 0
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-5",
                                        0
                                    ],
                                    "destination": [
                                        "obj-21",
                                        0
                                    ],
                                    "order": 0
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-7",
                                        0
                                    ],
                                    "destination": [
                                        "obj-21",
                                        0
                                    ],
                                    "order": 0
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-9",
                                        0
                                    ],
                                    "destination": [
                                        "obj-21",
                                        0
                                    ],
                                    "order": 0
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-11",
                                        0
                                    ],
                                    "destination": [
                                        "obj-21",
                                        0
                                    ],
                                    "order": 0
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-13",
                                        0
                                    ],
                                    "destination": [
                                        "obj-21",
                                        0
                                    ],
                                    "order": 0
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-15",
                                        0
                                    ],
                                    "destination": [
                                        "obj-21",
                                        0
                                    ],
                                    "order": 0
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-21",
                                        2
                                    ],
                                    "destination": [
                                        "obj-26",
                                        0
                                    ],
                                    "order": 0
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-26",
                                        0
                                    ],
                                    "destination": [
                                        "obj-21",
                                        0
                                    ],
                                    "order": 0
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-21",
                                        2
                                    ],
                                    "destination": [
                                        "obj-27",
                                        0
                                    ],
                                    "order": 0
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-27",
                                        0
                                    ],
                                    "destination": [
                                        "obj-21",
                                        0
                                    ],
                                    "order": 0
                                }
                            }
                        ],
                        "dependency_cache": [],
                        "autosave": 0,
                        "showontab": 1
                    },
                    "text": "p channels",
                    "outlettype": [
                        ""
                    ]
                }
            },
            {
                "box": {
                    "id": "obj-3",
                    "maxclass": "newobj",
                    "numinlets": 0,
                    "numoutlets": 0,
                    "patching_rect": [
                        235.0,
                        60.0,
                        100.0,
                        22.0
                    ],
                    "patcher": {
                        "fileversion": 1,
                        "appversion": {
                            "major": 8,
                            "minor": 5,
                            "revision": 5,
                            "architecture": "x64",
                            "modernui": 1
                        },
                        "classnamespace": "box",
                        "rect": [
                            0.0,
                            26.0,
                            720.0,
                            680.0
                        ],
                        "bglocked": 0,
                        "openinpresentation": 0,
                        "default_fontsize": 12.0,
                        "default_fontface": 0,
                        "default_fontname": "Arial",
                        "gridonopen": 1,
                        "gridsize": [
                            15.0,
                            15.0
                        ],
                        "gridsnaponopen": 1,
                        "objectsnaponopen": 1,
                        "statusbarvisible": 2,
                        "toolbarvisible": 1,
                        "lefttoolbarpinned": 0,
                        "toptoolbarpinned": 0,
                        "righttoolbarpinned": 0,
                        "bottomtoolbarpinned": 0,
                        "toolbars_unpinned_last_save": 0,
                        "tallnewobj": 0,
                        "boxanimatetime": 200,
                        "enablehscroll": 1,
                        "enablevscroll": 1,
                        "devicewidth": 0.0,
                        "description": "",
                        "digest": "",
                        "tags": "",
                        "style": "",
                        "subpatcher_template": "",
                        "assistshowspatchername": 0,
                        "boxes": [
                            {
                                "box": {
                                    "id": "obj-1",
                                    "maxclass": "comment",
                                    "numinlets": 1,
                                    "numoutlets": 0,
                                    "patching_rect": [
                                        15.0,
                                        12.0,
                                        640.0,
                                        28.0
                                    ],
                                    "text": "The buffer~ sample rate",
                                    "fontsize": 16.0
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-2",
                                    "maxclass": "comment",
                                    "numinlets": 1,
                                    "numoutlets": 0,
                                    "patching_rect": [
                                        15.0,
                                        48.0,
                                        660.0,
                                        36.0
                                    ],
                                    "text": "Like groove~, softkut~ plays a buffer~ at its own sample rate: rate 1 is the recorded speed, whatever the DSP rate."
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-3",
                                    "maxclass": "message",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "patching_rect": [
                                        15.0,
                                        98.0,
                                        268.0,
                                        22.0
                                    ],
                                    "text": "loopstart 0 0, loopend 0 1, play 0 1",
                                    "outlettype": [
                                        ""
                                    ]
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-4",
                                    "maxclass": "message",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "patching_rect": [
                                        15.0,
                                        128.0,
                                        72.0,
                                        22.0
                                    ],
                                    "text": "play 0 0",
                                    "outlettype": [
                                        ""
                                    ]
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-5",
                                    "maxclass": "comment",
                                    "numinlets": 1,
                                    "numoutlets": 0,
                                    "patching_rect": [
                                        15.0,
                                        158.0,
                                        380.0,
                                        51.0
                                    ],
                                    "text": "Loop times are seconds at the buffer~'s rate. After sr 22050, loopend 1 covers 22050 frames, so the loop window in the view doubles."
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-6",
                                    "maxclass": "message",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "patching_rect": [
                                        15.0,
                                        217.0,
                                        51.0,
                                        22.0
                                    ],
                                    "text": "reset",
                                    "outlettype": [
                                        ""
                                    ]
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-7",
                                    "maxclass": "comment",
                                    "numinlets": 1,
                                    "numoutlets": 0,
                                    "patching_rect": [
                                        74.0,
                                        217.0,
                                        100.0,
                                        21.0
                                    ],
                                    "text": "start over"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-8",
                                    "maxclass": "newobj",
                                    "numinlets": 1,
                                    "numoutlets": 1,
                                    "patching_rect": [
                                        480.0,
                                        98.0,
                                        72.0,
                                        22.0
                                    ],
                                    "text": "loadbang",
                                    "outlettype": [
                                        "bang"
                                    ]
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-9",
                                    "maxclass": "message",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "patching_rect": [
                                        480.0,
                                        128.0,
                                        156.0,
                                        22.0
                                    ],
                                    "text": "replace cello-f2.aif",
                                    "outlettype": [
                                        ""
                                    ]
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-10",
                                    "maxclass": "message",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "patching_rect": [
                                        480.0,
                                        158.0,
                                        72.0,
                                        22.0
                                    ],
                                    "text": "sr 44100",
                                    "outlettype": [
                                        ""
                                    ]
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-11",
                                    "maxclass": "message",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "patching_rect": [
                                        560.0,
                                        158.0,
                                        72.0,
                                        22.0
                                    ],
                                    "text": "sr 22050",
                                    "outlettype": [
                                        ""
                                    ]
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-12",
                                    "maxclass": "message",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "patching_rect": [
                                        640.0,
                                        158.0,
                                        72.0,
                                        22.0
                                    ],
                                    "text": "sr 88200",
                                    "outlettype": [
                                        ""
                                    ]
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-13",
                                    "maxclass": "newobj",
                                    "numinlets": 1,
                                    "numoutlets": 2,
                                    "patching_rect": [
                                        480.0,
                                        188.0,
                                        114.0,
                                        22.0
                                    ],
                                    "text": "buffer~ skh_sr",
                                    "outlettype": [
                                        "float",
                                        "bang"
                                    ]
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-14",
                                    "maxclass": "comment",
                                    "numinlets": 1,
                                    "numoutlets": 0,
                                    "patching_rect": [
                                        480.0,
                                        218.0,
                                        220.0,
                                        51.0
                                    ],
                                    "text": "sr relabels the buffer~'s rate: 22050 plays an octave down, 88200 an octave up."
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-15",
                                    "maxclass": "newobj",
                                    "numinlets": 1,
                                    "numoutlets": 2,
                                    "patching_rect": [
                                        15.0,
                                        285.0,
                                        121.0,
                                        22.0
                                    ],
                                    "text": "softkut~ skh_sr",
                                    "outlettype": [
                                        "signal",
                                        ""
                                    ]
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-16",
                                    "maxclass": "newobj",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "patching_rect": [
                                        480.0,
                                        285.0,
                                        58.0,
                                        22.0
                                    ],
                                    "text": "*~ 0.5",
                                    "outlettype": [
                                        "signal"
                                    ]
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-17",
                                    "maxclass": "ezdac~",
                                    "numinlets": 2,
                                    "numoutlets": 0,
                                    "patching_rect": [
                                        546.0,
                                        285.0,
                                        45.0,
                                        45.0
                                    ],
                                    "parameter_enable": 0
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-18",
                                    "maxclass": "comment",
                                    "numinlets": 1,
                                    "numoutlets": 0,
                                    "patching_rect": [
                                        599.0,
                                        285.0,
                                        90.0,
                                        21.0
                                    ],
                                    "text": "audio on/off"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-19",
                                    "maxclass": "bpatcher",
                                    "numinlets": 1,
                                    "numoutlets": 1,
                                    "patching_rect": [
                                        15.0,
                                        338.0,
                                        520.0,
                                        180.0
                                    ],
                                    "name": "softkut.view.maxpat",
                                    "args": [
                                        "skh_sr",
                                        1,
                                        0
                                    ],
                                    "outlettype": [
                                        ""
                                    ],
                                    "offset": [
                                        0.0,
                                        0.0
                                    ],
                                    "viewvisibility": 1,
                                    "bgmode": 0,
                                    "border": 0,
                                    "clickthrough": 0,
                                    "enablehscroll": 0,
                                    "enablevscroll": 0,
                                    "lockeddragscroll": 0
                                }
                            }
                        ],
                        "lines": [
                            {
                                "patchline": {
                                    "source": [
                                        "obj-8",
                                        0
                                    ],
                                    "destination": [
                                        "obj-9",
                                        0
                                    ],
                                    "order": 0
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-9",
                                        0
                                    ],
                                    "destination": [
                                        "obj-13",
                                        0
                                    ],
                                    "order": 0
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-10",
                                        0
                                    ],
                                    "destination": [
                                        "obj-13",
                                        0
                                    ],
                                    "order": 0
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-11",
                                        0
                                    ],
                                    "destination": [
                                        "obj-13",
                                        0
                                    ],
                                    "order": 0
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-12",
                                        0
                                    ],
                                    "destination": [
                                        "obj-13",
                                        0
                                    ],
                                    "order": 0
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-15",
                                        0
                                    ],
                                    "destination": [
                                        "obj-16",
                                        0
                                    ],
                                    "order": 0
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-16",
                                        0
                                    ],
                                    "destination": [
                                        "obj-17",
                                        0
                                    ],
                                    "order": 0
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-16",
                                        0
                                    ],
                                    "destination": [
                                        "obj-17",
                                        1
                                    ],
                                    "order": 1
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-3",
                                        0
                                    ],
                                    "destination": [
                                        "obj-15",
                                        0
                                    ],
                                    "order": 0
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-4",
                                        0
                                    ],
                                    "destination": [
                                        "obj-15",
                                        0
                                    ],
                                    "order": 0
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-6",
                                        0
                                    ],
                                    "destination": [
                                        "obj-15",
                                        0
                                    ],
                                    "order": 0
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-15",
                                        1
                                    ],
                                    "destination": [
                                        "obj-19",
                                        0
                                    ],
                                    "order": 0
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-19",
                                        0
                                    ],
                                    "destination": [
                                        "obj-15",
                                        0
                                    ],
                                    "order": 0
                                }
                            }
                        ],
                        "dependency_cache": [],
                        "autosave": 0,
                        "showontab": 1
                    },
                    "text": "p samplerate",
                    "outlettype": [
                        ""
                    ]
                }
            },
            {
                "box": {
                    "id": "obj-4",
                    "maxclass": "newobj",
                    "numinlets": 0,
                    "numoutlets": 0,
                    "patching_rect": [
                        345.0,
                        60.0,
                        100.0,
                        22.0
                    ],
                    "patcher": {
                        "fileversion": 1,
                        "appversion": {
                            "major": 8,
                            "minor": 5,
                            "revision": 5,
                            "architecture": "x64",
                            "modernui": 1
                        },
                        "classnamespace": "box",
                        "rect": [
                            0.0,
                            26.0,
                            720.0,
                            680.0
                        ],
                        "bglocked": 0,
                        "openinpresentation": 0,
                        "default_fontsize": 12.0,
                        "default_fontface": 0,
                        "default_fontname": "Arial",
                        "gridonopen": 1,
                        "gridsize": [
                            15.0,
                            15.0
                        ],
                        "gridsnaponopen": 1,
                        "objectsnaponopen": 1,
                        "statusbarvisible": 2,
                        "toolbarvisible": 1,
                        "lefttoolbarpinned": 0,
                        "toptoolbarpinned": 0,
                        "righttoolbarpinned": 0,
                        "bottomtoolbarpinned": 0,
                        "toolbars_unpinned_last_save": 0,
                        "tallnewobj": 0,
                        "boxanimatetime": 200,
                        "enablehscroll": 1,
                        "enablevscroll": 1,
                        "devicewidth": 0.0,
                        "description": "",
                        "digest": "",
                        "tags": "",
                        "style": "",
                        "subpatcher_template": "",
                        "assistshowspatchername": 0,
                        "boxes": [
                            {
                                "box": {
                                    "id": "obj-1",
                                    "maxclass": "comment",
                                    "numinlets": 1,
                                    "numoutlets": 0,
                                    "patching_rect": [
                                        15.0,
                                        12.0,
                                        640.0,
                                        28.0
                                    ],
                                    "text": "Record and overdub",
                                    "fontsize": 16.0
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-2",
                                    "maxclass": "comment",
                                    "numinlets": 1,
                                    "numoutlets": 0,
                                    "patching_rect": [
                                        15.0,
                                        48.0,
                                        660.0,
                                        36.0
                                    ],
                                    "text": "The signal inlet is the voice's record input. prelevel sets how much of the old content survives each pass. Watch the waveform fill in."
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-3",
                                    "maxclass": "message",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "patching_rect": [
                                        15.0,
                                        98.0,
                                        226.0,
                                        22.0
                                    ],
                                    "text": "loopend 0 2, rec 0 1, play 0 1",
                                    "outlettype": [
                                        ""
                                    ]
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-4",
                                    "maxclass": "message",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "patching_rect": [
                                        15.0,
                                        128.0,
                                        65.0,
                                        22.0
                                    ],
                                    "text": "rec 0 0",
                                    "outlettype": [
                                        ""
                                    ]
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-5",
                                    "maxclass": "comment",
                                    "numinlets": 1,
                                    "numoutlets": 0,
                                    "patching_rect": [
                                        88.0,
                                        128.0,
                                        200.0,
                                        21.0
                                    ],
                                    "text": "stop recording, keep looping"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-6",
                                    "maxclass": "message",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "patching_rect": [
                                        15.0,
                                        158.0,
                                        100.0,
                                        22.0
                                    ],
                                    "text": "prelevel 0 0",
                                    "outlettype": [
                                        ""
                                    ]
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-7",
                                    "maxclass": "message",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "patching_rect": [
                                        123.0,
                                        158.0,
                                        114.0,
                                        22.0
                                    ],
                                    "text": "prelevel 0 0.7",
                                    "outlettype": [
                                        ""
                                    ]
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-8",
                                    "maxclass": "message",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "patching_rect": [
                                        245.0,
                                        158.0,
                                        100.0,
                                        22.0
                                    ],
                                    "text": "prelevel 0 1",
                                    "outlettype": [
                                        ""
                                    ]
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-9",
                                    "maxclass": "comment",
                                    "numinlets": 1,
                                    "numoutlets": 0,
                                    "patching_rect": [
                                        15.0,
                                        188.0,
                                        380.0,
                                        21.0
                                    ],
                                    "text": "prelevel: 0 overwrites, 1 sums on top, between decays"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-10",
                                    "maxclass": "message",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "patching_rect": [
                                        15.0,
                                        217.0,
                                        51.0,
                                        22.0
                                    ],
                                    "text": "reset",
                                    "outlettype": [
                                        ""
                                    ]
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-11",
                                    "maxclass": "comment",
                                    "numinlets": 1,
                                    "numoutlets": 0,
                                    "patching_rect": [
                                        74.0,
                                        217.0,
                                        300.0,
                                        36.0
                                    ],
                                    "text": "start over (the recording stays in the buffer~)"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-12",
                                    "maxclass": "flonum",
                                    "numinlets": 1,
                                    "numoutlets": 2,
                                    "patching_rect": [
                                        480.0,
                                        98.0,
                                        60.0,
                                        22.0
                                    ],
                                    "outlettype": [
                                        "",
                                        "bang"
                                    ],
                                    "parameter_enable": 0
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-13",
                                    "maxclass": "comment",
                                    "numinlets": 1,
                                    "numoutlets": 0,
                                    "patching_rect": [
                                        548.0,
                                        98.0,
                                        50.0,
                                        21.0
                                    ],
                                    "text": "pitch"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-14",
                                    "maxclass": "newobj",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "patching_rect": [
                                        480.0,
                                        128.0,
                                        86.0,
                                        22.0
                                    ],
                                    "text": "cycle~ 220",
                                    "outlettype": [
                                        "signal"
                                    ]
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-15",
                                    "maxclass": "newobj",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "patching_rect": [
                                        574.0,
                                        128.0,
                                        72.0,
                                        22.0
                                    ],
                                    "text": "cycle~ 3",
                                    "outlettype": [
                                        "signal"
                                    ]
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-16",
                                    "maxclass": "newobj",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "patching_rect": [
                                        480.0,
                                        158.0,
                                        34.0,
                                        22.0
                                    ],
                                    "text": "*~",
                                    "outlettype": [
                                        "signal"
                                    ]
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-17",
                                    "maxclass": "comment",
                                    "numinlets": 1,
                                    "numoutlets": 0,
                                    "patching_rect": [
                                        522.0,
                                        158.0,
                                        150.0,
                                        21.0
                                    ],
                                    "text": "input: a pulsing tone"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-18",
                                    "maxclass": "message",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "patching_rect": [
                                        480.0,
                                        188.0,
                                        51.0,
                                        22.0
                                    ],
                                    "text": "clear",
                                    "outlettype": [
                                        ""
                                    ]
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-19",
                                    "maxclass": "newobj",
                                    "numinlets": 1,
                                    "numoutlets": 2,
                                    "patching_rect": [
                                        480.0,
                                        218.0,
                                        156.0,
                                        22.0
                                    ],
                                    "text": "buffer~ skh_rec 3000",
                                    "outlettype": [
                                        "float",
                                        "bang"
                                    ]
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-20",
                                    "maxclass": "newobj",
                                    "numinlets": 1,
                                    "numoutlets": 2,
                                    "patching_rect": [
                                        15.0,
                                        269.0,
                                        128.0,
                                        22.0
                                    ],
                                    "text": "softkut~ skh_rec",
                                    "outlettype": [
                                        "signal",
                                        ""
                                    ]
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-21",
                                    "maxclass": "newobj",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "patching_rect": [
                                        480.0,
                                        269.0,
                                        58.0,
                                        22.0
                                    ],
                                    "text": "*~ 0.5",
                                    "outlettype": [
                                        "signal"
                                    ]
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-22",
                                    "maxclass": "ezdac~",
                                    "numinlets": 2,
                                    "numoutlets": 0,
                                    "patching_rect": [
                                        546.0,
                                        269.0,
                                        45.0,
                                        45.0
                                    ],
                                    "parameter_enable": 0
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-23",
                                    "maxclass": "comment",
                                    "numinlets": 1,
                                    "numoutlets": 0,
                                    "patching_rect": [
                                        599.0,
                                        269.0,
                                        90.0,
                                        21.0
                                    ],
                                    "text": "audio on/off"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-24",
                                    "maxclass": "bpatcher",
                                    "numinlets": 1,
                                    "numoutlets": 1,
                                    "patching_rect": [
                                        15.0,
                                        322.0,
                                        520.0,
                                        180.0
                                    ],
                                    "name": "softkut.view.maxpat",
                                    "args": [
                                        "skh_rec",
                                        1,
                                        0
                                    ],
                                    "outlettype": [
                                        ""
                                    ],
                                    "offset": [
                                        0.0,
                                        0.0
                                    ],
                                    "viewvisibility": 1,
                                    "bgmode": 0,
                                    "border": 0,
                                    "clickthrough": 0,
                                    "enablehscroll": 0,
                                    "enablevscroll": 0,
                                    "lockeddragscroll": 0
                                }
                            }
                        ],
                        "lines": [
                            {
                                "patchline": {
                                    "source": [
                                        "obj-12",
                                        0
                                    ],
                                    "destination": [
                                        "obj-14",
                                        0
                                    ],
                                    "order": 0
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-14",
                                        0
                                    ],
                                    "destination": [
                                        "obj-16",
                                        0
                                    ],
                                    "order": 0
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-15",
                                        0
                                    ],
                                    "destination": [
                                        "obj-16",
                                        1
                                    ],
                                    "order": 0
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-18",
                                        0
                                    ],
                                    "destination": [
                                        "obj-19",
                                        0
                                    ],
                                    "order": 0
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-20",
                                        0
                                    ],
                                    "destination": [
                                        "obj-21",
                                        0
                                    ],
                                    "order": 0
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-21",
                                        0
                                    ],
                                    "destination": [
                                        "obj-22",
                                        0
                                    ],
                                    "order": 0
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-21",
                                        0
                                    ],
                                    "destination": [
                                        "obj-22",
                                        1
                                    ],
                                    "order": 1
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-3",
                                        0
                                    ],
                                    "destination": [
                                        "obj-20",
                                        0
                                    ],
                                    "order": 0
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-4",
                                        0
                                    ],
                                    "destination": [
                                        "obj-20",
                                        0
                                    ],
                                    "order": 0
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-6",
                                        0
                                    ],
                                    "destination": [
                                        "obj-20",
                                        0
                                    ],
                                    "order": 0
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-7",
                                        0
                                    ],
                                    "destination": [
                                        "obj-20",
                                        0
                                    ],
                                    "order": 0
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-8",
                                        0
                                    ],
                                    "destination": [
                                        "obj-20",
                                        0
                                    ],
                                    "order": 0
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-10",
                                        0
                                    ],
                                    "destination": [
                                        "obj-20",
                                        0
                                    ],
                                    "order": 0
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-16",
                                        0
                                    ],
                                    "destination": [
                                        "obj-20",
                                        0
                                    ],
                                    "order": 0
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-20",
                                        1
                                    ],
                                    "destination": [
                                        "obj-24",
                                        0
                                    ],
                                    "order": 0
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-24",
                                        0
                                    ],
                                    "destination": [
                                        "obj-20",
                                        0
                                    ],
                                    "order": 0
                                }
                            }
                        ],
                        "dependency_cache": [],
                        "autosave": 0,
                        "showontab": 1
                    },
                    "text": "p record",
                    "outlettype": [
                        ""
                    ]
                }
            },
            {
                "box": {
                    "id": "obj-5",
                    "maxclass": "newobj",
                    "numinlets": 0,
                    "numoutlets": 0,
                    "patching_rect": [
                        455.0,
                        60.0,
                        100.0,
                        22.0
                    ],
                    "patcher": {
                        "fileversion": 1,
                        "appversion": {
                            "major": 8,
                            "minor": 5,
                            "revision": 5,
                            "architecture": "x64",
                            "modernui": 1
                        },
                        "classnamespace": "box",
                        "rect": [
                            0.0,
                            26.0,
                            720.0,
                            680.0
                        ],
                        "bglocked": 0,
                        "openinpresentation": 0,
                        "default_fontsize": 12.0,
                        "default_fontface": 0,
                        "default_fontname": "Arial",
                        "gridonopen": 1,
                        "gridsize": [
                            15.0,
                            15.0
                        ],
                        "gridsnaponopen": 1,
                        "objectsnaponopen": 1,
                        "statusbarvisible": 2,
                        "toolbarvisible": 1,
                        "lefttoolbarpinned": 0,
                        "toptoolbarpinned": 0,
                        "righttoolbarpinned": 0,
                        "bottomtoolbarpinned": 0,
                        "toolbars_unpinned_last_save": 0,
                        "tallnewobj": 0,
                        "boxanimatetime": 200,
                        "enablehscroll": 1,
                        "enablevscroll": 1,
                        "devicewidth": 0.0,
                        "description": "",
                        "digest": "",
                        "tags": "",
                        "style": "",
                        "subpatcher_template": "",
                        "assistshowspatchername": 0,
                        "boxes": [
                            {
                                "box": {
                                    "id": "obj-1",
                                    "maxclass": "comment",
                                    "numinlets": 1,
                                    "numoutlets": 0,
                                    "patching_rect": [
                                        15.0,
                                        12.0,
                                        640.0,
                                        28.0
                                    ],
                                    "text": "Position reports",
                                    "fontsize": 16.0
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-2",
                                    "maxclass": "comment",
                                    "numinlets": 1,
                                    "numoutlets": 0,
                                    "patching_rect": [
                                        15.0,
                                        48.0,
                                        660.0,
                                        36.0
                                    ],
                                    "text": "The right outlet reports the play head. report <ms> starts or stops phase messages at once, also while audio runs. The view at the bottom is driven by these reports."
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-3",
                                    "maxclass": "message",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "patching_rect": [
                                        15.0,
                                        98.0,
                                        163.0,
                                        22.0
                                    ],
                                    "text": "loopend 0 2, play 0 1",
                                    "outlettype": [
                                        ""
                                    ]
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-4",
                                    "maxclass": "message",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "patching_rect": [
                                        15.0,
                                        128.0,
                                        79.0,
                                        22.0
                                    ],
                                    "text": "report 30",
                                    "outlettype": [
                                        ""
                                    ]
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-5",
                                    "maxclass": "message",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "patching_rect": [
                                        102.0,
                                        128.0,
                                        72.0,
                                        22.0
                                    ],
                                    "text": "report 0",
                                    "outlettype": [
                                        ""
                                    ]
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-6",
                                    "maxclass": "message",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "patching_rect": [
                                        182.0,
                                        128.0,
                                        100.0,
                                        22.0
                                    ],
                                    "text": "quant 0 0.25",
                                    "outlettype": [
                                        ""
                                    ]
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-7",
                                    "maxclass": "message",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "patching_rect": [
                                        290.0,
                                        128.0,
                                        79.0,
                                        22.0
                                    ],
                                    "text": "quant 0 0",
                                    "outlettype": [
                                        ""
                                    ]
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-8",
                                    "maxclass": "comment",
                                    "numinlets": 1,
                                    "numoutlets": 0,
                                    "patching_rect": [
                                        15.0,
                                        158.0,
                                        400.0,
                                        36.0
                                    ],
                                    "text": "report 0 freezes the play head; quant moves it in 0.25 s steps"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-9",
                                    "maxclass": "message",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "patching_rect": [
                                        15.0,
                                        202.0,
                                        44.0,
                                        22.0
                                    ],
                                    "text": "poll",
                                    "outlettype": [
                                        ""
                                    ]
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-10",
                                    "maxclass": "comment",
                                    "numinlets": 1,
                                    "numoutlets": 0,
                                    "patching_rect": [
                                        67.0,
                                        202.0,
                                        380.0,
                                        36.0
                                    ],
                                    "text": "poll: one info list per voice: voice, position 0-1 in the loop, play, rec, start ms, end ms, window ms, state"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-11",
                                    "maxclass": "message",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "patching_rect": [
                                        15.0,
                                        246.0,
                                        51.0,
                                        22.0
                                    ],
                                    "text": "reset",
                                    "outlettype": [
                                        ""
                                    ]
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-12",
                                    "maxclass": "comment",
                                    "numinlets": 1,
                                    "numoutlets": 0,
                                    "patching_rect": [
                                        74.0,
                                        246.0,
                                        100.0,
                                        21.0
                                    ],
                                    "text": "start over"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-13",
                                    "maxclass": "newobj",
                                    "numinlets": 1,
                                    "numoutlets": 1,
                                    "patching_rect": [
                                        480.0,
                                        98.0,
                                        72.0,
                                        22.0
                                    ],
                                    "text": "loadbang",
                                    "outlettype": [
                                        "bang"
                                    ]
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-14",
                                    "maxclass": "message",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "patching_rect": [
                                        480.0,
                                        128.0,
                                        156.0,
                                        22.0
                                    ],
                                    "text": "replace cello-f2.aif",
                                    "outlettype": [
                                        ""
                                    ]
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-15",
                                    "maxclass": "newobj",
                                    "numinlets": 1,
                                    "numoutlets": 2,
                                    "patching_rect": [
                                        480.0,
                                        158.0,
                                        121.0,
                                        22.0
                                    ],
                                    "text": "buffer~ skh_rep",
                                    "outlettype": [
                                        "float",
                                        "bang"
                                    ]
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-16",
                                    "maxclass": "newobj",
                                    "numinlets": 1,
                                    "numoutlets": 2,
                                    "patching_rect": [
                                        15.0,
                                        284.0,
                                        128.0,
                                        22.0
                                    ],
                                    "text": "softkut~ skh_rep",
                                    "outlettype": [
                                        "signal",
                                        ""
                                    ]
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-17",
                                    "maxclass": "newobj",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "patching_rect": [
                                        480.0,
                                        284.0,
                                        58.0,
                                        22.0
                                    ],
                                    "text": "*~ 0.5",
                                    "outlettype": [
                                        "signal"
                                    ]
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-18",
                                    "maxclass": "ezdac~",
                                    "numinlets": 2,
                                    "numoutlets": 0,
                                    "patching_rect": [
                                        546.0,
                                        284.0,
                                        45.0,
                                        45.0
                                    ],
                                    "parameter_enable": 0
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-19",
                                    "maxclass": "comment",
                                    "numinlets": 1,
                                    "numoutlets": 0,
                                    "patching_rect": [
                                        599.0,
                                        284.0,
                                        90.0,
                                        21.0
                                    ],
                                    "text": "audio on/off"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-20",
                                    "maxclass": "newobj",
                                    "numinlets": 3,
                                    "numoutlets": 3,
                                    "patching_rect": [
                                        15.0,
                                        337.0,
                                        128.0,
                                        22.0
                                    ],
                                    "text": "route phase info",
                                    "outlettype": [
                                        "",
                                        "",
                                        ""
                                    ]
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-21",
                                    "maxclass": "newobj",
                                    "numinlets": 1,
                                    "numoutlets": 2,
                                    "patching_rect": [
                                        151.0,
                                        337.0,
                                        93.0,
                                        22.0
                                    ],
                                    "text": "unpack 0 0.",
                                    "outlettype": [
                                        "int",
                                        "float"
                                    ]
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-22",
                                    "maxclass": "flonum",
                                    "numinlets": 1,
                                    "numoutlets": 2,
                                    "patching_rect": [
                                        252.0,
                                        337.0,
                                        70.0,
                                        22.0
                                    ],
                                    "outlettype": [
                                        "",
                                        "bang"
                                    ],
                                    "parameter_enable": 0
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-23",
                                    "maxclass": "comment",
                                    "numinlets": 1,
                                    "numoutlets": 0,
                                    "patching_rect": [
                                        330.0,
                                        337.0,
                                        70.0,
                                        21.0
                                    ],
                                    "text": "phase (s)"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-24",
                                    "maxclass": "newobj",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "patching_rect": [
                                        15.0,
                                        367.0,
                                        93.0,
                                        22.0
                                    ],
                                    "text": "prepend set",
                                    "outlettype": [
                                        ""
                                    ]
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-25",
                                    "maxclass": "message",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "patching_rect": [
                                        116.0,
                                        367.0,
                                        300.0,
                                        22.0
                                    ],
                                    "text": "",
                                    "outlettype": [
                                        ""
                                    ]
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-26",
                                    "maxclass": "comment",
                                    "numinlets": 1,
                                    "numoutlets": 0,
                                    "patching_rect": [
                                        424.0,
                                        367.0,
                                        90.0,
                                        21.0
                                    ],
                                    "text": "latest info"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-27",
                                    "maxclass": "bpatcher",
                                    "numinlets": 1,
                                    "numoutlets": 1,
                                    "patching_rect": [
                                        15.0,
                                        397.0,
                                        520.0,
                                        180.0
                                    ],
                                    "name": "softkut.view.maxpat",
                                    "args": [
                                        "skh_rep",
                                        1,
                                        0
                                    ],
                                    "outlettype": [
                                        ""
                                    ],
                                    "offset": [
                                        0.0,
                                        0.0
                                    ],
                                    "viewvisibility": 1,
                                    "bgmode": 0,
                                    "border": 0,
                                    "clickthrough": 0,
                                    "enablehscroll": 0,
                                    "enablevscroll": 0,
                                    "lockeddragscroll": 0
                                }
                            }
                        ],
                        "lines": [
                            {
                                "patchline": {
                                    "source": [
                                        "obj-13",
                                        0
                                    ],
                                    "destination": [
                                        "obj-14",
                                        0
                                    ],
                                    "order": 0
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-14",
                                        0
                                    ],
                                    "destination": [
                                        "obj-15",
                                        0
                                    ],
                                    "order": 0
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-16",
                                        0
                                    ],
                                    "destination": [
                                        "obj-17",
                                        0
                                    ],
                                    "order": 0
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-17",
                                        0
                                    ],
                                    "destination": [
                                        "obj-18",
                                        0
                                    ],
                                    "order": 0
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-17",
                                        0
                                    ],
                                    "destination": [
                                        "obj-18",
                                        1
                                    ],
                                    "order": 1
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-3",
                                        0
                                    ],
                                    "destination": [
                                        "obj-16",
                                        0
                                    ],
                                    "order": 0
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-4",
                                        0
                                    ],
                                    "destination": [
                                        "obj-16",
                                        0
                                    ],
                                    "order": 0
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-5",
                                        0
                                    ],
                                    "destination": [
                                        "obj-16",
                                        0
                                    ],
                                    "order": 0
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-6",
                                        0
                                    ],
                                    "destination": [
                                        "obj-16",
                                        0
                                    ],
                                    "order": 0
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-7",
                                        0
                                    ],
                                    "destination": [
                                        "obj-16",
                                        0
                                    ],
                                    "order": 0
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-9",
                                        0
                                    ],
                                    "destination": [
                                        "obj-16",
                                        0
                                    ],
                                    "order": 0
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-11",
                                        0
                                    ],
                                    "destination": [
                                        "obj-16",
                                        0
                                    ],
                                    "order": 0
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-16",
                                        1
                                    ],
                                    "destination": [
                                        "obj-20",
                                        0
                                    ],
                                    "order": 0
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-20",
                                        0
                                    ],
                                    "destination": [
                                        "obj-21",
                                        0
                                    ],
                                    "order": 0
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-21",
                                        1
                                    ],
                                    "destination": [
                                        "obj-22",
                                        0
                                    ],
                                    "order": 0
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-20",
                                        1
                                    ],
                                    "destination": [
                                        "obj-24",
                                        0
                                    ],
                                    "order": 0
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-24",
                                        0
                                    ],
                                    "destination": [
                                        "obj-25",
                                        0
                                    ],
                                    "order": 0
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-16",
                                        1
                                    ],
                                    "destination": [
                                        "obj-27",
                                        0
                                    ],
                                    "order": 0
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-27",
                                        0
                                    ],
                                    "destination": [
                                        "obj-16",
                                        0
                                    ],
                                    "order": 0
                                }
                            }
                        ],
                        "dependency_cache": [],
                        "autosave": 0,
                        "showontab": 1
                    },
                    "text": "p reports",
                    "outlettype": [
                        ""
                    ]
                }
            },
            {
                "box": {
                    "id": "obj-6",
                    "maxclass": "newobj",
                    "numinlets": 0,
                    "numoutlets": 0,
                    "patching_rect": [
                        565.0,
                        60.0,
                        100.0,
                        22.0
                    ],
                    "patcher": {
                        "fileversion": 1,
                        "appversion": {
                            "major": 8,
                            "minor": 5,
                            "revision": 5,
                            "architecture": "x64",
                            "modernui": 1
                        },
                        "classnamespace": "box",
                        "rect": [
                            0.0,
                            26.0,
                            720.0,
                            680.0
                        ],
                        "bglocked": 0,
                        "openinpresentation": 0,
                        "default_fontsize": 12.0,
                        "default_fontface": 0,
                        "default_fontname": "Arial",
                        "gridonopen": 1,
                        "gridsize": [
                            15.0,
                            15.0
                        ],
                        "gridsnaponopen": 1,
                        "objectsnaponopen": 1,
                        "statusbarvisible": 2,
                        "toolbarvisible": 1,
                        "lefttoolbarpinned": 0,
                        "toptoolbarpinned": 0,
                        "righttoolbarpinned": 0,
                        "bottomtoolbarpinned": 0,
                        "toolbars_unpinned_last_save": 0,
                        "tallnewobj": 0,
                        "boxanimatetime": 200,
                        "enablehscroll": 1,
                        "enablevscroll": 1,
                        "devicewidth": 0.0,
                        "description": "",
                        "digest": "",
                        "tags": "",
                        "style": "",
                        "subpatcher_template": "",
                        "assistshowspatchername": 0,
                        "boxes": [
                            {
                                "box": {
                                    "id": "obj-1",
                                    "maxclass": "comment",
                                    "numinlets": 1,
                                    "numoutlets": 0,
                                    "patching_rect": [
                                        15.0,
                                        12.0,
                                        640.0,
                                        28.0
                                    ],
                                    "text": "Safe values",
                                    "fontsize": 16.0
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-2",
                                    "maxclass": "comment",
                                    "numinlets": 1,
                                    "numoutlets": 0,
                                    "patching_rect": [
                                        15.0,
                                        48.0,
                                        660.0,
                                        36.0
                                    ],
                                    "text": "softcut checks no values. softkut~ clamps those that would crash it or write NaN into the buffer~, and posts a warning to the Max console."
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-3",
                                    "maxclass": "message",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "patching_rect": [
                                        15.0,
                                        98.0,
                                        130.0,
                                        22.0
                                    ],
                                    "text": "rate 0 100",
                                    "outlettype": [
                                        ""
                                    ]
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-4",
                                    "maxclass": "comment",
                                    "numinlets": 1,
                                    "numoutlets": 0,
                                    "patching_rect": [
                                        153.0,
                                        98.0,
                                        260.0,
                                        21.0
                                    ],
                                    "text": "rate is limited to -64..64"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-5",
                                    "maxclass": "message",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "patching_rect": [
                                        15.0,
                                        128.0,
                                        130.0,
                                        22.0
                                    ],
                                    "text": "postrq 0 -1",
                                    "outlettype": [
                                        ""
                                    ]
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-6",
                                    "maxclass": "comment",
                                    "numinlets": 1,
                                    "numoutlets": 0,
                                    "patching_rect": [
                                        153.0,
                                        128.0,
                                        260.0,
                                        21.0
                                    ],
                                    "text": "filter rq is at least 0.01"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-7",
                                    "maxclass": "message",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "patching_rect": [
                                        15.0,
                                        158.0,
                                        130.0,
                                        22.0
                                    ],
                                    "text": "prelevel 0 1.5",
                                    "outlettype": [
                                        ""
                                    ]
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-8",
                                    "maxclass": "comment",
                                    "numinlets": 1,
                                    "numoutlets": 0,
                                    "patching_rect": [
                                        153.0,
                                        158.0,
                                        260.0,
                                        21.0
                                    ],
                                    "text": "rec and pre levels are 0..1"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-9",
                                    "maxclass": "message",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "patching_rect": [
                                        15.0,
                                        188.0,
                                        130.0,
                                        22.0
                                    ],
                                    "text": "loopstart 0 -1",
                                    "outlettype": [
                                        ""
                                    ]
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-10",
                                    "maxclass": "comment",
                                    "numinlets": 1,
                                    "numoutlets": 0,
                                    "patching_rect": [
                                        153.0,
                                        188.0,
                                        260.0,
                                        21.0
                                    ],
                                    "text": "positions are 0 or more"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-11",
                                    "maxclass": "message",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "patching_rect": [
                                        15.0,
                                        218.0,
                                        130.0,
                                        22.0
                                    ],
                                    "text": "recpreslew 0 -1",
                                    "outlettype": [
                                        ""
                                    ]
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-12",
                                    "maxclass": "comment",
                                    "numinlets": 1,
                                    "numoutlets": 0,
                                    "patching_rect": [
                                        153.0,
                                        218.0,
                                        260.0,
                                        21.0
                                    ],
                                    "text": "times and slews are 0 or more"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-13",
                                    "maxclass": "message",
                                    "numinlets": 2,
                                    "numoutlets": 1,
                                    "patching_rect": [
                                        15.0,
                                        248.0,
                                        130.0,
                                        22.0
                                    ],
                                    "text": "reset",
                                    "outlettype": [
                                        ""
                                    ]
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-14",
                                    "maxclass": "comment",
                                    "numinlets": 1,
                                    "numoutlets": 0,
                                    "patching_rect": [
                                        153.0,
                                        248.0,
                                        260.0,
                                        21.0
                                    ],
                                    "text": "restore the defaults"
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-15",
                                    "maxclass": "newobj",
                                    "numinlets": 1,
                                    "numoutlets": 2,
                                    "patching_rect": [
                                        15.0,
                                        278.0,
                                        149.0,
                                        22.0
                                    ],
                                    "text": "softkut~ skh_limits",
                                    "outlettype": [
                                        "signal",
                                        ""
                                    ]
                                }
                            },
                            {
                                "box": {
                                    "id": "obj-16",
                                    "maxclass": "newobj",
                                    "numinlets": 1,
                                    "numoutlets": 2,
                                    "patching_rect": [
                                        172.0,
                                        278.0,
                                        177.0,
                                        22.0
                                    ],
                                    "text": "buffer~ skh_limits 1000",
                                    "outlettype": [
                                        "float",
                                        "bang"
                                    ]
                                }
                            }
                        ],
                        "lines": [
                            {
                                "patchline": {
                                    "source": [
                                        "obj-3",
                                        0
                                    ],
                                    "destination": [
                                        "obj-15",
                                        0
                                    ],
                                    "order": 0
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-5",
                                        0
                                    ],
                                    "destination": [
                                        "obj-15",
                                        0
                                    ],
                                    "order": 0
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-7",
                                        0
                                    ],
                                    "destination": [
                                        "obj-15",
                                        0
                                    ],
                                    "order": 0
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-9",
                                        0
                                    ],
                                    "destination": [
                                        "obj-15",
                                        0
                                    ],
                                    "order": 0
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-11",
                                        0
                                    ],
                                    "destination": [
                                        "obj-15",
                                        0
                                    ],
                                    "order": 0
                                }
                            },
                            {
                                "patchline": {
                                    "source": [
                                        "obj-13",
                                        0
                                    ],
                                    "destination": [
                                        "obj-15",
                                        0
                                    ],
                                    "order": 0
                                }
                            }
                        ],
                        "dependency_cache": [],
                        "autosave": 0,
                        "showontab": 1
                    },
                    "text": "p limits",
                    "outlettype": [
                        ""
                    ]
                }
            }
        ],
        "lines": [],
        "dependency_cache": [],
        "autosave": 0,
        "showrootpatcherontab": 0,
        "showontab": 0
    }
}