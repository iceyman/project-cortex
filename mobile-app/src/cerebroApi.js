// src/cerebroApi.js — all communication with Cerebro server
import AsyncStorage from '@react-native-async-storage/async-storage';

const DEFAULT_IP   = 'YOUR_CEREBRO_IP';
const DEFAULT_PORT = '5005';

export async function getCerebroUrl() {
  const ip   = await AsyncStorage.getItem('cerebro_ip')   || DEFAULT_IP;
  const port = await AsyncStorage.getItem('cerebro_port') || DEFAULT_PORT;
  return `http://${ip}:${port}`;
}

export async function saveCerebroUrl(ip, port) {
  await AsyncStorage.setItem('cerebro_ip',   ip);
  await AsyncStorage.setItem('cerebro_port', port);
}

export async function fetchHealth() {
  const base = await getCerebroUrl();
  const res  = await fetch(`${base}/health`, { signal: AbortSignal.timeout(4000) });
  return res.json();
}

export async function sendAudioChat(pcmBuffer, robotId, mode, kidName) {
  const base = await getCerebroUrl();
  const res  = await fetch(`${base}/chat`, {
    method:  'POST',
    headers: {
      'Content-Type':    'application/octet-stream',
      'X-Session':       robotId,
      'X-Robot-ID':      robotId,
      'X-Mode':          mode,
      'X-Kid-Name':      kidName,
      'X-Sample-Rate':   '16000',
    },
    body: pcmBuffer,
  });
  return res;
}

export async function dashboardSay(robotId, text) {
  const base = await getCerebroUrl();
  return fetch(`${base}/say`, {
    method:  'POST',
    headers: { 'Content-Type': 'application/json' },
    body:    JSON.stringify({ robot_id: robotId, text }),
  });
}

export async function setMode(robotId, mode) {
  const base = await getCerebroUrl();
  return fetch(`${base}/mode`, {
    method:  'POST',
    headers: { 'Content-Type': 'application/json' },
    body:    JSON.stringify({ robot_id: robotId, mode }),
  });
}

export async function setVolume(robotId, volume) {
  const base = await getCerebroUrl();
  return fetch(`${base}/volume`, {
    method:  'POST',
    headers: { 'Content-Type': 'application/json' },
    body:    JSON.stringify({ robot_id: robotId, volume }),
  });
}

export async function forceSleep() {
  const base = await getCerebroUrl();
  return fetch(`${base}/sleep`, { method: 'POST' });
}

export async function forceWake() {
  const base = await getCerebroUrl();
  return fetch(`${base}/wake`, { method: 'POST' });
}

export async function forceOtaUpdate(robotId) {
  const base = await getCerebroUrl();
  return fetch(`${base}/ota/force`, {
    method:  'POST',
    headers: { 'Content-Type': 'application/json' },
    body:    JSON.stringify({ robot_id: robotId }),
  });
}

export async function resetMemory(robotId) {
  const base = await getCerebroUrl();
  return fetch(`${base}/reset`, {
    method:  'POST',
    headers: {
      'X-Session':  robotId,
      'X-Robot-ID': robotId,
    },
  });
}

export async function getRecentHistory(robotId, n = 3) {
  const base = await getCerebroUrl();
  const res  = await fetch(`${base}/recent_history?robot_id=${robotId}&n=${n}`);
  return res.json();
}
