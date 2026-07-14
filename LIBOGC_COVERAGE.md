# libogc-Abdeckung durch `wiitools`

Vergleich der relevanten Homebrew-APIs aus `/opt/devkitpro/libogc/gc` mit dem,
was das `wiitools`-Python-Modul (`Modules/wiitoolsmodule.c`) bereitstellt.

**Legende**

| Symbol | Bedeutung |
|--------|-----------|
| ✅ | Direkt als Python-Funktion in `wiitools` erreichbar |
| ⚙️ | Intern von `wiitools` genutzt, aber **nicht** als Python-API exponiert |
| ❌ | Nicht erreichbar (weder exponiert noch intern verwendet) |

> Hinweis: Nur die für Wii-Homebrew praxisrelevanten Header sind aufgeführt
> (WPAD, PAD, VIDEO, SYSTEM, CONF, AUDIO, CONSOLE, GU, GX, NETWORK, SDCARD).
> `ogc/gx.h` enthält allein ~1000 Low-Level-Funktionen — dort sind nur die von
> `wiitools` intern verwendeten gelistet, der Rest ist pauschal nicht exponiert.

---

## `wiiuse/wpad.h` — Wiimote (WPAD)

Vollständig als Python-API abgedeckt.

| Funktion | wiitools |
|----------|:--------:|
| `WPAD_Init` | ✅ |
| `WPAD_ScanPads` | ✅ |
| `WPAD_ButtonsDown` | ✅ |
| `WPAD_ButtonsUp` | ✅ |
| `WPAD_ButtonsHeld` | ✅ |
| `WPAD_Data` | ✅ |
| `WPAD_Accel` | ✅ |
| `WPAD_GForce` | ✅ |
| `WPAD_Orientation` | ✅ |
| `WPAD_IR` | ✅ |
| `WPAD_Expansion` | ✅ |
| `WPAD_Probe` | ✅ |
| `WPAD_SetDataFormat` | ✅ |
| `WPAD_SetVRes` | ✅ |
| `WPAD_Rumble` | ✅ |
| `WPAD_Disconnect` | ✅ |
| `WPAD_Flush` | ✅ |
| `WPAD_GetStatus` | ✅ |
| `WPAD_BatteryLevel` | ✅ |
| `WPAD_ControlSpeaker` | ✅ |
| `WPAD_IsSpeakerEnabled` | ✅ |
| `WPAD_SendStreamData` | ✅ |
| `WPAD_EncodeData` | ✅ |
| `WPAD_SetMotionPlus` | ✅ |
| `WPAD_SetEventBufs` | ✅ |
| `WPAD_ReadEvent` | ✅ |
| `WPAD_ReadPending` | ✅ |
| `WPAD_DroppedEvents` | ✅ |
| `WPAD_SetIdleThresholds` | ✅ |
| `WPAD_SetIdleTimeout` | ✅ |
| `WPAD_SetBatteryDeadCallback` | ✅ |
| `WPAD_SetPowerButtonCallback` | ✅ |
| `WPAD_Shutdown` | ✅ |

> Zusätzlich bietet `wiitools` die Nicht-libogc-Helfer `WPAD_GetBatteryDeadEvent`
> und `WPAD_GetPowerButtonEvent` sowie `*_all`-Varianten für alle 4 Kanäle.

---

## `ogc/pad.h` — GameCube-Controller (PAD)

| Funktion | wiitools |
|----------|:--------:|
| `PAD_Init` | ✅ |
| `PAD_ScanPads` | ✅ |
| `PAD_ButtonsDown` | ✅ |
| `PAD_ButtonsUp` | ✅ |
| `PAD_ButtonsHeld` | ✅ |
| `PAD_Read` | ✅ |
| `PAD_Sync` | ✅ |
| `PAD_Reset` | ✅ |
| `PAD_Recalibrate` | ✅ |
| `PAD_SetSpec` | ✅ |
| `PAD_Clamp` | ✅ |
| `PAD_ControlMotor` | ✅ |
| `PAD_StickX` | ✅ |
| `PAD_StickY` | ✅ |
| `PAD_SubStickX` | ✅ |
| `PAD_SubStickY` | ✅ |
| `PAD_TriggerL` | ✅ |
| `PAD_TriggerR` | ✅ |
| `PAD_SetSamplingCallback` + `PAD_GetSamplingEvent` | ✅ (Poll) |

---

## `ogc/video.h` — Video (VIDEO)

Nur `VIDEO_WaitVSync` ist als Python-API exponiert. Die Video-Initialisierung
erledigt `wiitools` intern (`video_init_custom`, `init`).

| Funktion | wiitools |
|----------|:--------:|
| `VIDEO_WaitVSync` | ✅ |
| `VIDEO_Init` | ⚙️ |
| `VIDEO_Configure` | ⚙️ |
| `VIDEO_Flush` | ⚙️ |
| `VIDEO_GetCurrentTvMode` | ⚙️ |
| `VIDEO_GetPreferredMode` | ⚙️ |
| `VIDEO_SetBlack` | ⚙️ |
| `VIDEO_SetNextFramebuffer` | ⚙️ |
| `VIDEO_SetPreRetraceCallback` | ⚙️ |
| `VIDEO_ClearFrameBuffer` | ❌ |
| `VIDEO_GetCurrentFramebuffer` | ❌ |
| `VIDEO_GetCurrentLine` | ❌ |
| `VIDEO_GetFrameBufferSize` | ❌ |
| `VIDEO_GetNextField` | ❌ |
| `VIDEO_GetNextFramebuffer` | ❌ |
| `VIDEO_GetVideoScanMode` | ❌ |
| `VIDEO_HaveComponentCable` | ❌ |
| `VIDEO_SetNextRightFramebuffer` | ❌ |
| `VIDEO_SetPostRetraceCallback` | ❌ |

---

## `ogc/system.h` — System (SYS)

Die nützliche/sichere Teilmenge ist exponiert. Callbacks laufen im Interrupt-Kontext
und werden per Poll-Muster angeboten (`SYS_Set…Callback(enable)` + `SYS_Get…Event()`).
Speicher-/Low-Level-Funktionen bleiben absichtlich unerreichbar (Footgun-Gefahr — sie
würden bei falschem Aufruf den Heap zerstören oder abstürzen).

| Funktion | wiitools |
|----------|:--------:|
| `SYS_Time` | ✅ |
| `SYS_ResetButtonDown` | ✅ |
| `SYS_GetHollywoodRevision` | ✅ |
| `SYS_Report` / `SYS_STDIO_Report` | ✅ |
| `SYS_ResetSystem` | ✅ (inkl. `SYS_RETURNTOMENU`/`SYS_POWEROFF`-Konstanten) |
| `SYS_GetCounterBias` / `SYS_SetCounterBias` | ✅ |
| `SYS_GetDisplayOffsetH` / `SYS_SetDisplayOffsetH` | ✅ |
| `SYS_GetEuRGB60` / `SYS_SetEuRGB60` | ✅ |
| `SYS_GetLanguage` / `SYS_SetLanguage` | ✅ |
| `SYS_GetProgressiveScan` / `SYS_SetProgressiveScan` | ✅ |
| `SYS_GetSoundMode` / `SYS_SetSoundMode` | ✅ |
| `SYS_GetVideoMode` / `SYS_SetVideoMode` | ✅ |
| `SYS_GetWirelessID` / `SYS_SetWirelessID` | ✅ |
| `SYS_GetGBSMode` / `SYS_SetGBSMode` | ✅ |
| `SYS_GetFontEncoding` | ✅ |
| `SYS_GetArena1Size` / `SYS_GetArena2Size` | ✅ (nur lesend) |
| `SYS_SetPowerCallback` + `SYS_GetPowerEvent` | ✅ (Poll) |
| `SYS_SetResetCallback` + `SYS_GetResetEvent` | ✅ (Poll) |
| `SYS_CreateAlarm` / `SYS_SetAlarm` / `SYS_SetPeriodicAlarm` / `SYS_RemoveAlarm` / `SYS_CancelAlarm` + `SYS_GetAlarmEvent` | ✅ (Poll) |
| `SYS_AllocateFramebuffer` | ⚙️ (intern) |
| `SYS_Init` | ❌ (läuft beim Boot) |
| `SYS_GetArena1Hi/Lo`, `SYS_SetArena1Hi/Lo`, Arena2 Hi/Lo | ❌ (Heap-Grenzen — Footgun) |
| `SYS_RegisterResetFunc` / `SYS_UnregisterResetFunc` | ❌ (braucht Struct) |
| `SYS_InitFont` / `SYS_GetFontTexel` / `SYS_GetFontTexture` | ❌ (Font-Puffer) |
| `SYS_ProtectRange` | ❌ |
| `SYS_StartPMC` / `SYS_StopPMC` / `SYS_ResetPMC` / `SYS_DumpPMC` | ❌ (Perf-Counter) |
| `SYS_SwitchFiber` | ❌ (würde abstürzen) |
| `kprintf` / `dietPrintV` | ❌ |

---

## `ogc/conf.h` — Systemkonfiguration (CONF)

**Alle lesenden `CONF_*`-Funktionen sind jetzt als Python-API exponiert** (conf.h enthält
ohnehin nur Lesezugriffe). Schreibende Pendants liegen in `SYS_Set*` (siehe SYSTEM).

| Funktion | wiitools |
|----------|:--------:|
| `CONF_Init` | ✅ |
| `CONF_Get` / `CONF_GetLength` / `CONF_GetType` | ✅ |
| `CONF_GetAspectRatio` | ✅ (auch intern) |
| `CONF_GetDisplayOffsetH` | ✅ (auch intern) |
| `CONF_GetArea` / `CONF_GetRegion` / `CONF_GetLanguage` | ✅ |
| `CONF_GetNickName` | ✅ (→ `str`) |
| `CONF_GetVideo` / `CONF_GetEuRGB60` / `CONF_GetProgressiveScan` | ✅ |
| `CONF_GetSoundMode` | ✅ |
| `CONF_GetCounterBias` | ✅ |
| `CONF_GetIRSensitivity` / `CONF_GetSensorBarPosition` | ✅ |
| `CONF_GetPadDevices` / `CONF_GetPadMotorMode` / `CONF_GetPadSpeakerVolume` | ✅ |
| `CONF_GetScreenSaverMode` / `CONF_GetShutdownMode` / `CONF_GetIdleLedMode` | ✅ |
| `CONF_GetParentalAnswer` / `CONF_GetParentalPassword` / `CONF_GetEULA` | ✅ |
| `CONF_GetWiiConnect24` | ✅ |

---

## `ogc/audio.h` — Audio (AUDIO)

**DMA/DSP-API vollständig exponiert.** Die **Stream-API** (`*Stream*`) ist im
Wii-libogc **nicht implementiert** (existiert nur im GameCube-Build) und daher nicht
wrappbar.

| Funktion | wiitools |
|----------|:--------:|
| `AUDIO_Init` | ✅ |
| `AUDIO_InitDMA` / `AUDIO_StartDMA` / `AUDIO_StopDMA` | ✅ |
| `AUDIO_GetDMABytesLeft` / `AUDIO_GetDMALength` / `AUDIO_GetDMAStartAddr` / `AUDIO_GetDMAEnableFlag` | ✅ |
| `AUDIO_SetDSPSampleRate` / `AUDIO_GetDSPSampleRate` | ✅ |
| `AUDIO_RegisterDMACallback` + `AUDIO_GetDMAEvent` | ✅ (Poll) |
| `AUDIO_*Stream*` (Vol/SampleRate/PlayState/Trigger/ResetSampleCnt/RegisterStreamCallback) | ❌ (GameCube-only, im Wii-libogc nicht vorhanden) |

---

## `ogc/consol.h` — Konsole (CON)

Die devkitPro-Konsole (`console_init`) wird intern zur Initialisierung genutzt;
die `CON_*`-API ist nicht exponiert.

| Funktion | wiitools |
|----------|:--------:|
| `console_init` (libogc) | ⚙️ |
| `CON_GetMetrics` | ✅ (→ `(cols, rows)`) |
| `CON_GetPosition` | ✅ (→ `(col, row)`) |
| `CON_EnableGecko` | ✅ |
| `CON_Init` | ❌ |
| `CON_InitEx` | ❌ |

---

## `ogc/gu.h` — Matrix-/Vektor-Mathematik (GU)

Nur die für die 2D-Projektion nötigen Funktionen werden intern genutzt.

| Funktion | wiitools |
|----------|:--------:|
| `guMtxIdentity` (`c_guMtxIdentity`) | ⚙️ |
| `guOrtho` | ⚙️ |
| Alle übrigen `gu*` / `c_gu*` / `ps_gu*` (Frustum, Perspective, LookAt, Quat, Vec, …) | ❌ |

---

## `ogc/gx.h` — GX-Grafikpipeline

Keine `GX_*`-Funktion ist als Python-API exponiert. `wiitools` kapselt das
Zeichnen komplett (`draw_rect`, `draw_circle`, `png_show`, `render_text`, …).
Intern verwendete GX-Funktionen (⚙️):

`GX_Init`, `GX_Begin`, `GX_End`, `GX_ClearVtxDesc`, `GX_SetVtxDesc`, `GX_SetVtxAttrFmt`,
`GX_SetArray`, `GX_Position3f32`, `GX_Color4u8`, `GX_TexCoord2f32`, `GX_LoadPosMtxImm`,
`GX_LoadProjectionMtx`, `GX_SetViewport`, `GX_SetScissor`, `GX_SetCullMode`,
`GX_SetBlendMode`, `GX_SetZMode`, `GX_SetZCompLoc`, `GX_SetColorUpdate`,
`GX_SetAlphaCompare`, `GX_SetNumChans`, `GX_SetNumTexGens`, `GX_SetNumTevStages`,
`GX_SetTevOp`, `GX_SetTevOrder`, `GX_SetTexCoordGen`, `GX_SetLineWidth`,
`GX_InitTexObj`, `GX_InitTexObjLOD`, `GX_LoadTexObj`, `GX_InvalidateTexAll`,
`GX_CopyDisp`, `GX_SetCopyClear`, `GX_SetCopyFilter`, `GX_SetDispCopySrc`,
`GX_SetDispCopyDst`, `GX_SetDispCopyGamma`, `GX_SetDispCopyYScale`,
`GX_GetYScaleFactor`, `GX_SetFieldMode`, `GX_DrawDone`, `GX_WaitDrawDone`

**Alle anderen ~1000 `GX_*`-Funktionen: ❌ (nicht erreichbar).**

---

## `network.h` — Netzwerk / Sockets (net)

**Weitgehend exponiert**, aber Pythonisch vereinfacht: IP/Port als `str`/`int` statt
`struct sockaddr`, MAC als `"aa:bb:cc:dd:ee:ff"`, `recv` liefert `bytes`, `accept`/
`recvfrom` liefern Tupel. Zusätzlich weiter verfügbar: die High-Level-Helfer
`curl_get/post/request`, `get_local_ip`, `IsNetReady`.

| Funktion | wiitools |
|----------|:--------:|
| `net_init` | ✅ (Thread) |
| `net_get_status` / `net_deinit` / `net_wc24cleanup` | ✅ |
| `net_gethostip` | ✅ (→ IP-`str`) |
| `net_get_mac_address` | ✅ (→ `"aa:bb:cc:dd:ee:ff"`) |
| `net_gethostbyname` | ✅ (→ IP-`str` / `None`) |
| `net_socket` / `net_close` / `net_shutdown` | ✅ |
| `net_bind` / `net_listen` / `net_accept` / `net_connect` | ✅ (IP+Port) |
| `net_read` / `net_write` / `net_send` / `net_recv` | ✅ (`bytes`) |
| `net_sendto` / `net_recvfrom` | ✅ (IP+Port, `bytes`) |
| `net_poll` / `net_fcntl` / `net_ioctl` | ✅ |
| `net_setsockopt` / `net_getsockname` | ✅ (int-Optionen) |
| `net_getsockopt` | ❌ (im Wii-libogc nicht implementiert; nur `net_setsockopt`) |
| `if_config` | ✅ (→ dict) |
| `inet_addr` / `inet_aton` / `inet_ntoa` | ✅ |
| `net_select` | ❌ (fd_set unpraktisch aus Python — `net_poll` verwenden) |
| `net_init_async` | ❌ (durch bestehenden `net_init`-Thread ersetzt) |
| `if_configex` | ❌ (Roh-Struct-Variante; `if_config` verwenden) |

---

## `sdcard/wiisd_io.h`, `sdcard/gcsd.h` — SD-Karte

| Symbol | wiitools |
|--------|:--------:|
| `__io_wiisd` (DISC_INTERFACE) | ⚙️ (via `fatInitDefault`) |
| `__io_gcsda` / `__io_gcsdb` / `__io_gcsd2` | ❌ |

> SD-Zugriff läuft in `wiitools` über libfat (`fatInitDefault`) und die normale
> Python-Datei-I/O (`open`, `read_file`, `write_file`, `remove`), nicht über die
> rohe Karten-Schnittstelle.

---

## Zusammenfassung

| Bereich | ✅ exponiert | ⚙️ intern | ❌ nicht erreichbar |
|---------|:-----------:|:--------:|:------------------:|
| WPAD (Wiimote) | 33 / 33 | – | 0 |
| PAD (GameCube) | 19 / 19 | – | 0 |
| VIDEO | 1 | 8 | 10 |
| SYSTEM | ~30 | 1 | ~15 (Heap/PMC/Fiber) |
| CONF | alle Get (25) | – | 0 |
| AUDIO | DMA/DSP (12) | – | 10 (Stream-API, GameCube-only) |
| CONSOLE | 3 | 1 | 2 |
| GU | 0 | 2 | Rest |
| GX | 0 | ~42 | ~1000 |
| NETWORK | ~29 | – | 4 (`net_select`, `net_init_async`, `if_configex`, `net_getsockopt`) |
| SDCARD | 0 | 1 | Rest |

**Kernaussage (nach Ausbau):** Vollständig abgedeckt sind jetzt **Eingabe** (WPAD/PAD),
**Systemeinstellungen** (CONF read + SYS Set/Get), **Audio** und die **rohe Socket-API**
(Pythonisch vereinfacht). Grafik (`GX`) und Video werden weiter über eigene High-Level-
APIs abstrahiert; nur Heap-/Perf-/Fiber-Interna von `SYSTEM` und die Roh-Struct-Varianten
von `network.h` bleiben bewusst unerreichbar.

> **Callback-Muster:** libogc-Callbacks (PAD-Sampling, SYS-Power/Reset, Alarme,
> Audio-DMA/Stream) feuern im Interrupt-Kontext, wo kein Python laufen darf. Sie werden
> daher über ein C-Trampolin nur gezählt; Python fragt den Zähler per `*_GetEvent()` ab
> (gleiches Muster wie bei WPAD-Power/Battery).
