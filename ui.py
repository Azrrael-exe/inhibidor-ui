# /// script
# requires-python = ">=3.11"
# dependencies = [
#   "streamlit>=1.35.0",
#   "requests>=2.31.0",
# ]
# ///

import json
import time
from datetime import datetime

import requests
import streamlit as st

# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------

TIMEOUT = 3  # seconds
AZ_MIN, AZ_MAX = 0.0, 450.0
EL_MIN, EL_MAX = 0.0, 180.0
NUM_BANDS = 7

COMPASS_LABELS = ["N ↑", "NE ↗", "E →", "SE ↘", "S ↓", "SW ↙", "W ←", "NW ↖"]


# ---------------------------------------------------------------------------
# HTTP client
# ---------------------------------------------------------------------------

def get_status(ip: str) -> tuple[dict | None, str | None]:
    try:
        r = requests.get(f"http://{ip}/status", timeout=TIMEOUT)
        if r.status_code == 200:
            return r.json(), None
        try:
            msg = r.json().get("error", r.text)
        except Exception:
            msg = r.text
        return None, f"HTTP {r.status_code}: {msg}"
    except requests.exceptions.ConnectionError:
        return None, "Connection refused — device offline or wrong IP"
    except requests.exceptions.Timeout:
        return None, f"Timeout after {TIMEOUT}s"
    except Exception as e:
        return None, f"Error: {e}"


def post_command(ip: str, payload: dict) -> tuple[str | None, str | None]:
    try:
        r = requests.post(
            f"http://{ip}/set-navigation-and-power",
            json=payload,
            timeout=TIMEOUT,
        )
        if r.status_code == 200:
            data = r.json()
            return data.get("status", "ok"), None
        try:
            msg = r.json().get("error", r.text)
        except Exception:
            msg = r.text
        return None, f"HTTP {r.status_code}: {msg}"
    except requests.exceptions.ConnectionError:
        return None, "Connection refused"
    except requests.exceptions.Timeout:
        return None, f"Timeout after {TIMEOUT}s"
    except Exception as e:
        return None, f"Error: {e}"


def post_hard_stop(ip: str) -> tuple[str | None, str | None]:
    try:
        r = requests.post(f"http://{ip}/hard-stop", json={}, timeout=TIMEOUT)
        if r.status_code == 200:
            data = r.json()
            return data.get("status", "ok"), None
        try:
            msg = r.json().get("error", r.text)
        except Exception:
            msg = r.text
        return None, f"HTTP {r.status_code}: {msg}"
    except requests.exceptions.ConnectionError:
        return None, "Connection refused"
    except requests.exceptions.Timeout:
        return None, f"Timeout after {TIMEOUT}s"
    except Exception as e:
        return None, f"Error: {e}"

# ---------------------------------------------------------------------------
# Session state
# ---------------------------------------------------------------------------

def init_session_state():
    defaults: dict = {
        "device_ip": "192.168.1.100",
        "last_status": None,
        "last_error": None,
        "last_cmd_result": None,   # ("success"|"error", message)
        "refresh_interval": 3,
        "auto_refresh": True,
        "ctrl_azimuth": 0.0,
        "ctrl_elevation": 0.0,
        "ctrl_bands": {f"band_{i}": False for i in range(NUM_BANDS)},
        "send_nav": True,
        "send_bands": True,
        # Network config command builder
        "netcfg_mode": "dhcp",
        "netcfg_ip": "192.168.1.100",
        "netcfg_subnet": "255.255.255.0",
        "netcfg_gateway": "192.168.1.1",
    }
    for k, v in defaults.items():
        if k not in st.session_state:
            st.session_state[k] = v

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def heading_label(degrees: float) -> str:
    idx = round(degrees / 45) % 8
    return COMPASS_LABELS[idx]


def fmt_datetime(raw: str) -> str:
    try:
        dt = datetime.fromisoformat(raw.replace("Z", "+00:00"))
        return dt.strftime("%Y-%m-%d %H:%M UTC")
    except Exception:
        return raw or "—"


def band_led(is_on: bool) -> str:
    color = "#00cc44" if is_on else "#555555"
    glow = f"0 0 8px {color}" if is_on else "none"
    return (
        f'<div style="width:28px;height:28px;border-radius:50%;'
        f'background:{color};margin:auto;box-shadow:{glow};"></div>'
    )

# ---------------------------------------------------------------------------
# Sidebar
# ---------------------------------------------------------------------------

def render_sidebar():
    with st.sidebar:
        st.title("Configuration")

        ip = st.text_input("Device IP", value=st.session_state.device_ip)
        st.session_state.device_ip = ip

        st.session_state.refresh_interval = st.number_input(
            "Refresh interval (s)", min_value=1, max_value=60,
            value=st.session_state.refresh_interval,
        )
        st.session_state.auto_refresh = st.toggle(
            "Auto-refresh", value=st.session_state.auto_refresh
        )

        st.divider()

        if st.session_state.last_error:
            st.markdown(
                '<span style="color:#cc3333;">● Disconnected</span>',
                unsafe_allow_html=True,
            )
            st.caption(st.session_state.last_error)
        elif st.session_state.last_status:
            st.markdown(
                '<span style="color:#00cc44;">● Connected</span>',
                unsafe_allow_html=True,
            )
        else:
            st.markdown(
                '<span style="color:#888888;">● Waiting…</span>',
                unsafe_allow_html=True,
            )

        if st.button("Refresh now", use_container_width=True):
            st.rerun()

        st.divider()
        render_network_config()

# ---------------------------------------------------------------------------
# Network config — JSON command builder
# ---------------------------------------------------------------------------

def render_network_config():
    with st.expander("Network Config (Command Builder)", expanded=False):
        st.caption(
            "Construye los comandos JSON para configurar la IP del dispositivo. "
            "Copialos y enviálos por Serial USB con `pio device monitor -b 115200` "
            "(recordá presionar Enter después de pegar)."
        )

        # ── Read current config ─────────────────────────────────────────────
        st.markdown("**1. Consultar configuración actual**")
        st.code('{"cmd":"get-config"}', language="json")

        st.divider()

        # ── Build a set-config command ──────────────────────────────────────
        st.markdown("**2. Configurar nueva IP**")
        mode = st.radio(
            "Mode", ["dhcp", "static"],
            index=0 if st.session_state.netcfg_mode == "dhcp" else 1,
            horizontal=True,
            key="netcfg_mode_radio",
        )
        st.session_state.netcfg_mode = mode

        if mode == "static":
            st.session_state.netcfg_ip = st.text_input(
                "IP", value=st.session_state.netcfg_ip, key="netcfg_ip_input",
            )
            st.session_state.netcfg_subnet = st.text_input(
                "Subnet mask", value=st.session_state.netcfg_subnet,
                key="netcfg_subnet_input",
            )
            st.session_state.netcfg_gateway = st.text_input(
                "Gateway", value=st.session_state.netcfg_gateway,
                key="netcfg_gateway_input",
            )
            payload = {
                "cmd": "set-config",
                "mode": "static",
                "ip": st.session_state.netcfg_ip.strip(),
                "subnet": st.session_state.netcfg_subnet.strip(),
                "gateway": st.session_state.netcfg_gateway.strip(),
            }
        else:
            payload = {"cmd": "set-config", "mode": "dhcp"}

        st.code(json.dumps(payload), language="json")

        st.divider()

        # ── Factory reset ───────────────────────────────────────────────────
        st.markdown("**3. Reset de fábrica (vuelve a DHCP)**")
        st.code('{"cmd":"reset-config"}', language="json")

# ---------------------------------------------------------------------------
# Status section
# ---------------------------------------------------------------------------

def render_status_section():
    st.subheader("System Status")

    status = st.session_state.last_status

    if st.session_state.last_error:
        st.warning(f"Status fetch failed: {st.session_state.last_error}")
        if status:
            st.caption("Showing last known state")

    if not status:
        st.info("Waiting for first response from device…")
        return

    # GPS
    st.markdown("**GPS**")
    gps = status.get("gps", {})
    c1, c2, c3, c4 = st.columns(4)
    c1.metric("Latitude", gps.get("lat", "—") + "°")
    c2.metric("Longitude", gps.get("lon", "—") + "°")
    c3.metric("Altitude", gps.get("alt", "—") + " m")
    c4.metric("UTC", fmt_datetime(gps.get("datetime", "")))

    st.divider()

    # Heading + Navigation
    st.markdown("**Orientation & Navigation**")
    c1, c2, c3 = st.columns(3)
    try:
        hdg = float(status.get("heading", 0))
        hdg_str = f"{hdg:.1f}°"
        hdg_card = heading_label(hdg)
    except Exception:
        hdg_str = "—"
        hdg_card = ""
    c1.metric("Heading", hdg_str, delta=hdg_card, delta_color="off")

    nav = status.get("navigation", {})
    try:
        az_str = f"{float(nav.get('azimuth', 0)):.1f}°"
    except Exception:
        az_str = "—"
    try:
        el_str = f"{float(nav.get('elevation', 0)):.1f}°"
    except Exception:
        el_str = "—"
    c2.metric("Azimuth", az_str)
    c3.metric("Elevation", el_str)

    st.divider()

    # RF Bands
    st.markdown("**RF Bands**")
    power = status.get("power", {})
    cols = st.columns(NUM_BANDS)
    for i, col in enumerate(cols):
        key = f"band_{i}"
        is_on = power.get(key, False)
        label = "ON" if is_on else "OFF"
        col.markdown(
            f'<div style="text-align:center;">'
            f'{band_led(is_on)}'
            f'<small>B{i}<br><b>{label}</b></small>'
            f'</div>',
            unsafe_allow_html=True,
        )

# ---------------------------------------------------------------------------
# Control panel
# ---------------------------------------------------------------------------

def render_control_panel():
    st.subheader("Control")

    if st.button("🚨 APAGADO DE EMERGENCIA 🚨", type="primary", use_container_width=True):
        for i in range(NUM_BANDS):
            st.session_state.ctrl_bands[f"band_{i}"] = False
            # Force visually unchecking the Streamlit widgets
            st.session_state[f"ctrl_band_{i}"] = False
            
        ok, err = post_hard_stop(st.session_state.device_ip)
        st.session_state.last_cmd_result = ("success", "🚨 EMERGENCIA: Parada del sistema (Hard Stop)") if ok else ("error", err)
        st.rerun()

    status = st.session_state.last_status
    current_power = status.get("power", {}) if status else {}

    send_nav = st.checkbox("Set navigation angles", value=st.session_state.send_nav)
    send_bands = st.checkbox("Set RF bands", value=st.session_state.send_bands)
    st.session_state.send_nav = send_nav
    st.session_state.send_bands = send_bands

    st.divider()

    # Navigation sliders
    az = st.slider(
        "Azimuth (°)", min_value=AZ_MIN, max_value=AZ_MAX,
        value=st.session_state.ctrl_azimuth, step=0.5,
        disabled=not send_nav,
    )
    el = st.slider(
        "Elevation (°)", min_value=EL_MIN, max_value=EL_MAX,
        value=st.session_state.ctrl_elevation, step=0.5,
        disabled=not send_nav,
    )
    st.session_state.ctrl_azimuth = az
    st.session_state.ctrl_elevation = el

    st.divider()

    # Band toggles (pre-filled from current device state)
    st.markdown("**RF Bands**")
    band_values: dict[str, bool] = {}
    cols = st.columns(4)
    for i in range(NUM_BANDS):
        key = f"band_{i}"
        default = current_power.get(key, st.session_state.ctrl_bands.get(key, False))
        band_values[key] = cols[i % 4].checkbox(
            f"Band {i}", value=default, key=f"ctrl_band_{i}",
            disabled=not send_bands,
        )
    st.session_state.ctrl_bands = band_values

    # Quick-action buttons
    if send_bands:
        qa1, qa2 = st.columns(2)
        if qa1.button("All ON", use_container_width=True):
            for k in band_values:
                band_values[k] = True
            st.session_state.ctrl_bands = band_values
            ok, err = post_command(
                st.session_state.device_ip,
                {k: True for k in band_values},
            )
            st.session_state.last_cmd_result = ("success", "All bands ON") if ok else ("error", err)
            st.rerun()
        if qa2.button("All OFF", use_container_width=True):
            for k in band_values:
                band_values[k] = False
            st.session_state.ctrl_bands = band_values
            ok, err = post_command(
                st.session_state.device_ip,
                {k: False for k in band_values},
            )
            st.session_state.last_cmd_result = ("success", "All bands OFF") if ok else ("error", err)
            st.rerun()

    st.divider()

    # Send button
    if st.button(
        "Send Command",
        type="primary",
        use_container_width=True,
        disabled=(not send_nav and not send_bands),
    ):
        payload: dict = {}
        if send_nav:
            payload["azimuth"] = az
            payload["elevation"] = el
        if send_bands:
            payload.update(band_values)

        if payload:
            ok, err = post_command(st.session_state.device_ip, payload)
            if ok:
                st.session_state.last_cmd_result = ("success", f"Command queued ({ok})")
            else:
                st.session_state.last_cmd_result = ("error", err)

    # Feedback
    if st.session_state.last_cmd_result:
        level, msg = st.session_state.last_cmd_result
        if level == "success":
            st.success(msg)
        else:
            st.error(msg)

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    st.set_page_config(
        page_title="RF Inhibitor Control",
        page_icon="📡",
        layout="wide",
        initial_sidebar_state="expanded",
    )
    init_session_state()
    render_sidebar()

    # Fetch status on every run
    data, err = get_status(st.session_state.device_ip)
    if data is not None:
        st.session_state.last_status = data
        st.session_state.last_error = None
    elif err:
        st.session_state.last_error = err

    st.title("RF Inhibitor Control Panel")

    left_col, right_col = st.columns([3, 2])

    with left_col:
        render_status_section()

    with right_col:
        render_control_panel()

    # Auto-refresh
    if st.session_state.auto_refresh:
        time.sleep(st.session_state.refresh_interval)
        st.rerun()


if __name__ == "__main__":
    import sys
    from streamlit import runtime
    from streamlit.web import cli as stcli

    if runtime.exists():
        main()
    else:
        sys.argv = ["streamlit", "run", __file__]
        sys.exit(stcli.main())
