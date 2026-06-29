# Pi Tape — FTP на Raspberry Pi + Deck

FTP-сервер работает **на самой Pi**. Deck загружает готовый Side A/B WAV и `manifest.json`; Pi воспроизводит файл на jack → линейный вход деки.

## Схема

```text
Deck (Mac/PC)  ──FTP upload──►  Raspberry Pi 4B (vsftpd)
                                      │
                                      ├─ /srv/pi-tape/inbox/
                                      └─ pi-tape-play.sh → aplay → jack → дека
```

## 1. Настройка Pi (один раз)

```bash
sudo apt update
sudo apt install -y vsftpd ffmpeg alsa-utils curl python3

sudo mkdir -p /srv/pi-tape/{inbox,work,archive,failed}
sudo useradd -m -d /srv/pi-tape -s /usr/sbin/nologin deck || true
echo 'deck:YOUR_PASSWORD' | sudo chpasswd

# vsftpd + HTTP control daemon
sudo install -m 755 pi-tape/scripts/pi-tape-daemon.py /usr/local/bin/pi-tape-daemon.py
sudo cp pi-tape/systemd/pi-tape-daemon.service /etc/systemd/system/
sudo systemctl enable --now pi-tape-daemon.service
sudo tee /etc/vsftpd.conf.d/deck.conf <<'EOF'
write_enable=YES
local_enable=YES
chroot_local_user=YES
allow_writeable_chroot=YES
user_sub_token=$USER
local_root=/srv/pi-tape
pasv_enable=YES
pasv_min_port=30000
pasv_max_port=30009
EOF

echo 'include=/etc/vsftpd.conf.d/deck.conf' | sudo tee -a /etc/vsftpd.conf
sudo systemctl restart vsftpd

sudo install -m 755 pi-tape/scripts/pi-tape-play.sh /usr/local/bin/pi-tape-play.sh
sudo cp pi-tape/config.example.env /etc/pi-tape/config.env
sudo sed -i 's/TRANSPORT=ftp/TRANSPORT=local/' /etc/pi-tape/config.env
sudo cp pi-tape/systemd/pi-tape-play.* /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now pi-tape-play.timer
```

`TRANSPORT=local` — Pi читает файлы из `/srv/pi-tape/inbox/` после FTP-загрузки из Deck (без повторного скачивания по сети).

## Deck (Send to Pi)

Фича **не входит в релизные сборки**. Только отдельная dev-сборка:

```bash
cmake --preset pi-dev
cmake --build --preset pi-dev-deck
```

или:

```bash
make configure-pi
make build-pi
```

**Локальные defaults (host и т.д.):** опционально `cmake/LocalPiTape.cmake` (в `.gitignore`).  
Этот файл **не включает** Pi tape сам по себе — только когда `CASSETTE_ENABLE_PI_TAPE=ON`.

Production / Applications / CI всегда:

```bash
cmake -C cmake/ProdOptions.cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target Deck
```

## 2. Настройка Deck

**Settings → Raspberry Pi tape deck:**

| Поле | Пример |
|------|--------|
| Host | `raspberrypi.local` или `192.168.1.20` |
| Port | `21` |
| User | `deck` |
| Password | ваш пароль |
| Remote folder | `inbox` |

Включите **Enable Send to Pi**.

## 3. Запись

1. В Deck: **Prepare** → кнопка **Pi Tape**.
2. **Upload queue** — Side A и Side B уходят на Pi (`tape_queue.json`).
3. На деке: REC + PLAY.
4. В диалоге Deck: **Play Side A** — прогресс-бар показывает текущую сторону.
5. **Stop** — остановить подачу с Pi.
6. Переверните кассету → **Play Side B**.

HTTP API Pi (порт 8765): `GET /api/status`, `POST /api/play`, `POST /api/stop`.

## Firewall на Pi (если включён ufw)

```bash
sudo ufw allow 21/tcp
sudo ufw allow 8765/tcp
sudo ufw allow 30000:30009/tcp
```

## Файлы

| Файл | Назначение |
|------|------------|
| `scripts/pi-tape-play.sh` | Конвертация + воспроизведение |
| `systemd/pi-tape-play.*` | Timer / oneshot service |
| `config.example.env` | Конфиг Pi |

Deck-сторона: `Source/io/FtpClient.cpp`, `PiTapeUploader.cpp`, кнопка **Send to Pi** в главном окне.
