#!/usr/bin/env bash
# Run on Raspberry Pi (as user with sudo). Installs vsftpd + pi-tape daemon.
set -euo pipefail

PI_TAPE_SRC="${1:-/tmp/pi-tape-setup}"
LAN_IP="${2:-192.168.1.119}"
DECK_PASS="${DECK_PASS:-deck}"

echo "[setup] Installing packages..."
sudo apt-get update -qq
sudo DEBIAN_FRONTEND=noninteractive apt-get install -y vsftpd ffmpeg alsa-utils curl python3

echo "[setup] Directories..."
sudo mkdir -p /srv/pi-tape/{inbox,work,archive,failed}
sudo mkdir -p /etc/pi-tape /var/lib/pi-tape

if ! id deck &>/dev/null; then
  sudo useradd -d /srv/pi-tape -s /usr/sbin/nologin deck
fi
echo "deck:${DECK_PASS}" | sudo chpasswd
sudo chown -R deck:deck /srv/pi-tape
sudo chmod 755 /srv/pi-tape
sudo chmod 775 /srv/pi-tape/inbox

echo "[setup] vsftpd..."
echo deck | sudo tee /etc/vsftpd.userlist >/dev/null

sudo cp /etc/vsftpd.conf "/etc/vsftpd.conf.bak.$(date +%s)" 2>/dev/null || true
sudo tee /etc/vsftpd.conf >/dev/null <<EOF
listen=YES
listen_ipv6=NO
anonymous_enable=NO
local_enable=YES
write_enable=YES
local_umask=022
check_shell=NO
dirmessage_enable=YES
use_localtime=YES
xferlog_enable=YES
connect_from_port_20=YES
chroot_local_user=YES
allow_writeable_chroot=YES
secure_chroot_dir=/var/run/vsftpd/empty
pam_service_name=vsftpd
pasv_enable=YES
pasv_min_port=30000
pasv_max_port=30009
pasv_address=${LAN_IP}
userlist_enable=YES
userlist_file=/etc/vsftpd.userlist
userlist_deny=NO
ssl_enable=NO
EOF

# deck uses nologin shell — disable pam_shells check for FTP
sudo sed -i 's/^auth.*pam_shells.so/#auth required pam_shells.so/' /etc/pam.d/vsftpd

echo "[setup] pi-tape scripts..."
sudo install -m 755 "${PI_TAPE_SRC}/scripts/pi-tape-daemon.py" /usr/local/bin/pi-tape-daemon.py
sudo install -m 755 "${PI_TAPE_SRC}/scripts/pi-tape-play.sh" /usr/local/bin/pi-tape-play.sh
sudo cp "${PI_TAPE_SRC}/config.example.env" /etc/pi-tape/config.env

sudo cp "${PI_TAPE_SRC}/systemd/pi-tape-daemon.service" /etc/systemd/system/
sudo cp "${PI_TAPE_SRC}/systemd/pi-tape-play.service" /etc/systemd/system/
sudo cp "${PI_TAPE_SRC}/systemd/pi-tape-play.timer" /etc/systemd/system/

sudo systemctl daemon-reload
sudo systemctl enable --now pi-tape-daemon.service
sudo systemctl enable --now pi-tape-play.timer
sudo systemctl restart vsftpd

if command -v ufw >/dev/null && sudo ufw status 2>/dev/null | grep -q 'Status: active'; then
  sudo ufw allow 21/tcp
  sudo ufw allow 8765/tcp
  sudo ufw allow 30000:30009/tcp
fi

echo "[setup] Done."
echo "FTP: deck@${LAN_IP} password=${DECK_PASS}  remote dir: inbox/"
echo "HTTP: http://${LAN_IP}:8765/api/status"
systemctl is-active vsftpd pi-tape-daemon.service
