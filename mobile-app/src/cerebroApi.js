import AsyncStorage from '@react-native-async-storage/async-storage';

const DEFAULT_IP   = 'YOUR_CEREBRO_IP';
const DEFAULT_PORT = '5005';

export async function getCerebroUrl() {
  const ip   = await AsyncStorage.getItem('cerebro_ip')   || DEFAULT_IP;
  const port = await AsyncStorage.getItem('cerebro_port') || DEFAULT_PORT;
  return 'http://' + ip + ':' + port;
}

export async function saveCerebroUrl(ip, port) {
  await AsyncStorage.setItem('cerebro_ip', ip);
  await AsyncStorage.setItem('cerebro_port', port);
}

export async function fetchHealth() {
  const base = await getCerebroUrl();
  const res  = await fetch(base + '/health', { signal: AbortSignal.timeout(4000) });
  return res.json();
}

export async function sendAppVoice(audioData, robotId, mode, kidName) {
  const base = await getCerebroUrl();
  const res  = await fetch(base + '/web_chat?robot_id=' + robotId + '&mode=' + mode + '&kid_name=' + encodeURIComponent(kidName), {
    method:  'POST',
    headers: { 'Content-Type': 'application/octet-stream' },
    body:    audioData,
  });
  return res;
}

export async function dashboardSay(robotId, text) {
  const base = await getCerebroUrl();
  return fetch(base + '/say', {
    method:  'POST',
    headers: { 'Content-Type': 'application/json' },
    body:    JSON.stringify({ robot_id: robotId, text: text }),
  });
}

export async function setMode(robotId, mode) {
  const base = await getCerebroUrl();
  return fetch(base + '/mode', {
    method:  'POST',
    headers: { 'Content-Type': 'application/json' },
    body:    JSON.stringify({ robot_id: robotId, mode: mode }),
  });
}

export async function setVolume(robotId, volume) {
  const base = await getCerebroUrl();
  return fetch(base + '/volume', {
    method:  'POST',
    headers: { 'Content-Type': 'application/json' },
    body:    JSON.stringify({ robot_id: robotId, volume: volume }),
  });
}

export async function forceSleep() {
  const base = await getCerebroUrl();
  return fetch(base + '/sleep', { method: 'POST' });
}

export async function forceWake() {
  const base = await getCerebroUrl();
  return fetch(base + '/wake', { method: 'POST' });
}

export async function forceReboot(robotId) {
  const base = await getCerebroUrl();
  return fetch(base + '/reboot', {
    method:  'POST',
    headers: { 'Content-Type': 'application/json' },
    body:    JSON.stringify({ robot_id: robotId }),
  });
}

export async function forceOtaUpdate(robotId) {
  const base = await getCerebroUrl();
  return fetch(base + '/ota/force', {
    method:  'POST',
    headers: { 'Content-Type': 'application/json' },
    body:    JSON.stringify({ robot_id: robotId }),
  });
}

export async function resetMemory(robotId) {
  const base = await getCerebroUrl();
  return fetch(base + '/reset', {
    method:  'POST',
    headers: { 'X-Session': robotId, 'X-Robot-ID': robotId },
  });
}

export async function getRecentHistory(robotId, n) {
  const base = await getCerebroUrl();
  const res  = await fetch(base + '/recent_history?robot_id=' + robotId + '&n=' + (n || 3));
  return res.json();
}

export async function saveSDConfig(robotId, config) {
  const base = await getCerebroUrl();
  return fetch(base + '/config/set', {
    method:  'POST',
    headers: { 'Content-Type': 'application/json' },
    body:    JSON.stringify({ robot_id: robotId, ...config }),
  });
}
