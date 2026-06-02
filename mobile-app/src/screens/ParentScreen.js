// src/screens/ParentScreen.js
import React, { useState, useEffect, useCallback } from 'react';
import {
  View, Text, StyleSheet, ScrollView, Pressable,
  TextInput, RefreshControl, Switch, Alert,
} from 'react-native';
import {
  fetchHealth, dashboardSay, setMode, setVolume,
  forceSleep, forceWake, forceOtaUpdate, resetMemory,
  getRecentHistory,
} from '../cerebroApi';

const ROBOTS = [
  { id: 'robot1', name: 'Kira', color: '#8b5cf6', badge: '#5b21b6' },
  { id: 'robot2', name: 'Rumi', color: '#ff6eb4', badge: '#c2185b' },
];

export default function ParentScreen() {
  const [health,     setHealth]    = useState(null);
  const [histories,  setHistories] = useState({});
  const [sayText,    setSayText]   = useState({ robot1: '', robot2: '' });
  const [volumes,    setVolumes]   = useState({ robot1: 180, robot2: 180 });
  const [refreshing, setRefreshing] = useState(false);
  const [toast,      setToast]     = useState('');

  const showToast = msg => {
    setToast(msg);
    setTimeout(() => setToast(''), 3000);
  };

  const load = useCallback(async () => {
    try {
      const h = await fetchHealth();
      setHealth(h);
      for (const r of ROBOTS) {
        const hist = await getRecentHistory(r.id, 3);
        setHistories(prev => ({ ...prev, [r.id]: hist.history || [] }));
      }
    } catch (e) {
      showToast('❌ Cannot reach Cerebro');
    }
  }, []);

  useEffect(() => { load(); }, [load]);

  const onRefresh = async () => {
    setRefreshing(true);
    await load();
    setRefreshing(false);
  };

  const handleSay = async (robotId) => {
    const text = sayText[robotId]?.trim();
    if (!text) return;
    await dashboardSay(robotId, text);
    setSayText(prev => ({ ...prev, [robotId]: '' }));
    showToast('✅ Message queued!');
  };

  const handleMode = async (robotId, isKid) => {
    await setMode(robotId, isKid ? 'kid' : 'adult');
    showToast(`✅ ${isKid ? 'Kid' : 'Adult'} mode set`);
    load();
  };

  const handleVolume = async (robotId, vol) => {
    setVolumes(prev => ({ ...prev, [robotId]: vol }));
    await setVolume(robotId, vol);
  };

  const handleReset = (robotId) => {
    Alert.alert('Reset Memory', `Clear ${robotId}'s conversation history?`, [
      { text: 'Cancel', style: 'cancel' },
      { text: 'Reset', style: 'destructive', onPress: async () => {
        await resetMemory(robotId);
        showToast('🗑️ Memory cleared');
        load();
      }},
    ]);
  };

  return (
    <View style={styles.container}>
      {toast ? <View style={styles.toast}><Text style={styles.toastText}>{toast}</Text></View> : null}

      <ScrollView
        style={{ flex: 1, width: '100%' }}
        refreshControl={<RefreshControl refreshing={refreshing} onRefresh={onRefresh} tintColor="#8b5cf6" />}
        contentContainerStyle={{ padding: 16, gap: 14 }}
      >
        {/* Server status bar */}
        <View style={styles.statusBar}>
          <Text style={styles.statusItem}>
            <Text style={styles.dim}>Server </Text>
            <Text style={[styles.bold, { color: '#4caf50' }]}>online</Text>
          </Text>
          {health && <>
            <Text style={styles.statusItem}>
              <Text style={styles.dim}>Whisper </Text>
              <Text style={styles.bold}>{health.whisper}</Text>
            </Text>
            <Text style={styles.statusItem}>
              <Text style={[styles.bold, { color: health.sleeping ? '#7070c0' : '#4caf50' }]}>
                {health.sleeping ? '🌙 Sleeping' : '☀️ Awake'}
              </Text>
            </Text>
          </>}
        </View>

        {/* Global buttons */}
        <View style={styles.globalBtns}>
          <Pressable style={[styles.gbtn, { backgroundColor: '#1a1a38' }]}
            onPress={async () => { await forceSleep(); showToast('🌙 Sleep sent'); }}>
            <Text style={[styles.gbtnText, { color: '#7070c0' }]}>🌙 Sleep all</Text>
          </Pressable>
          <Pressable style={[styles.gbtn, { backgroundColor: '#1a381a' }]}
            onPress={async () => { await forceWake(); showToast('☀️ Wake sent'); }}>
            <Text style={[styles.gbtnText, { color: '#4caf50' }]}>☀️ Wake all</Text>
          </Pressable>
        </View>

        {/* Robot cards */}
        {ROBOTS.map(robot => {
          const hist   = histories[robot.id] || [];
          const isKid  = robot.id === 'robot2'; // Rumi default kid mode
          const vol    = volumes[robot.id];

          return (
            <View key={robot.id} style={[styles.card, { borderColor: robot.color + '44' }]}>
              {/* Card header */}
              <View style={styles.cardHeader}>
                <View style={[styles.dot, { backgroundColor: '#4caf50' }]} />
                <Text style={[styles.robotName, { color: robot.color }]}>{robot.name}</Text>
                <Pressable
                  style={[styles.updateBtn]}
                  onPress={async () => {
                    await forceOtaUpdate(robot.id);
                    showToast(`⬆️ Update queued for ${robot.name}`);
                  }}>
                  <Text style={styles.updateBtnText}>⬆️ Update</Text>
                </Pressable>
              </View>

              {/* Mode toggle */}
              <View style={styles.row}>
                <Text style={styles.label}>Kid mode</Text>
                <Switch
                  value={isKid}
                  onValueChange={v => handleMode(robot.id, v)}
                  trackColor={{ false: '#333', true: robot.color }}
                  thumbColor="white"
                />
              </View>

              {/* Volume */}
              <View style={styles.row}>
                <Text style={styles.label}>Volume: <Text style={{ color: robot.color }}>{vol}</Text></Text>
              </View>
              <View style={styles.volRow}>
                {[80, 120, 160, 200, 240].map(v => (
                  <Pressable key={v}
                    style={[styles.volBtn, vol === v && { backgroundColor: robot.color }]}
                    onPress={() => handleVolume(robot.id, v)}>
                    <Text style={[styles.volBtnText, vol === v && { color: 'white' }]}>{v}</Text>
                  </Pressable>
                ))}
              </View>

              {/* Quick actions */}
              <View style={styles.qrow}>
                {[
                  ['👋', 'Hey! How is everyone?'],
                  ['🕺', 'dance party'],
                  ['😂', 'Tell me a joke'],
                  ['🌙', 'Goodnight everyone, sweet dreams!'],
                  ['🌤️', 'What is the weather like today?'],
                ].map(([emoji, text]) => (
                  <Pressable key={emoji} style={styles.qbtn}
                    onPress={() => dashboardSay(robot.id, text).then(() => showToast('✅ Sent!'))}>
                    <Text style={styles.qbtnText}>{emoji}</Text>
                  </Pressable>
                ))}
              </View>

              {/* Say it */}
              <TextInput
                style={styles.input}
                placeholder={`Type something for ${robot.name}...`}
                placeholderTextColor="#444"
                value={sayText[robot.id]}
                onChangeText={t => setSayText(prev => ({ ...prev, [robot.id]: t }))}
                onSubmitEditing={() => handleSay(robot.id)}
                returnKeyType="send"
              />
              <View style={styles.btnRow}>
                <Pressable style={styles.sayBtn} onPress={() => handleSay(robot.id)}>
                  <Text style={styles.sayBtnText}>🔊 Say it</Text>
                </Pressable>
                <Pressable style={styles.resetBtn} onPress={() => handleReset(robot.id)}>
                  <Text style={styles.resetBtnText}>🗑️ Reset</Text>
                </Pressable>
              </View>

              {/* Recent history */}
              {hist.length > 0 && (
                <View style={styles.histBox}>
                  <Text style={styles.histTitle}>RECENT CHAT</Text>
                  {hist.map((h, i) => (
                    <View key={i} style={styles.histPair}>
                      <Text style={styles.histUser}>👤 {h.user}</Text>
                      <Text style={[styles.histBot, { color: robot.color }]}>{h.assistant}</Text>
                    </View>
                  ))}
                </View>
              )}
            </View>
          );
        })}

        <View style={{ height: 20 }} />
      </ScrollView>
    </View>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#0f0f13', alignItems: 'center' },
  toast: {
    position: 'absolute', top: 10, left: 20, right: 20, zIndex: 999,
    backgroundColor: '#1a3a1a', borderRadius: 12, padding: 12,
    borderWidth: 1, borderColor: '#4caf5088',
  },
  toastText: { color: '#90e0a0', fontWeight: '700', textAlign: 'center' },
  statusBar: {
    backgroundColor: '#1a1a24', borderRadius: 12, padding: 12,
    flexDirection: 'row', flexWrap: 'wrap', gap: 14,
    borderWidth: 1, borderColor: '#252535',
  },
  statusItem: { fontSize: 13 },
  dim:  { color: '#555' },
  bold: { color: '#a0a0c0', fontWeight: '700' },
  globalBtns: { flexDirection: 'row', gap: 10 },
  gbtn: { flex: 1, padding: 12, borderRadius: 10, alignItems: 'center' },
  gbtnText: { fontWeight: '700', fontSize: 14 },
  card: {
    backgroundColor: '#1a1a24', borderRadius: 16, padding: 16,
    borderWidth: 1, gap: 10,
  },
  cardHeader: { flexDirection: 'row', alignItems: 'center', gap: 8 },
  dot: { width: 9, height: 9, borderRadius: 5 },
  robotName: { fontSize: 22, fontWeight: '800', flex: 1 },
  updateBtn: { backgroundColor: '#1a2a1a', padding: 7, borderRadius: 8 },
  updateBtnText: { color: '#70c080', fontSize: 12, fontWeight: '700' },
  row: { flexDirection: 'row', justifyContent: 'space-between', alignItems: 'center' },
  label: { color: '#777', fontSize: 14, fontWeight: '600' },
  volRow: { flexDirection: 'row', gap: 6 },
  volBtn: {
    flex: 1, padding: 7, borderRadius: 8,
    backgroundColor: '#111', alignItems: 'center',
    borderWidth: 1, borderColor: '#222',
  },
  volBtnText: { color: '#555', fontSize: 12, fontWeight: '700' },
  qrow: { flexDirection: 'row', gap: 6 },
  qbtn: {
    flex: 1, padding: 9, borderRadius: 8,
    backgroundColor: '#1c1c2e', alignItems: 'center',
    borderWidth: 1, borderColor: '#2a2a42',
  },
  qbtnText: { fontSize: 18 },
  input: {
    backgroundColor: '#111118', borderWidth: 1, borderColor: '#222232',
    borderRadius: 10, padding: 10, color: '#e8e8f0', fontSize: 14,
  },
  btnRow: { flexDirection: 'row', gap: 8 },
  sayBtn:   { flex: 2, backgroundColor: '#2a4060', borderRadius: 9, padding: 10, alignItems: 'center' },
  resetBtn: { flex: 1, backgroundColor: '#3a1818', borderRadius: 9, padding: 10, alignItems: 'center' },
  sayBtnText:   { color: '#80b8f0', fontWeight: '700', fontSize: 13 },
  resetBtnText: { color: '#c07070', fontWeight: '700', fontSize: 13 },
  histBox: { backgroundColor: '#111118', borderRadius: 10, padding: 10, gap: 8 },
  histTitle: { fontSize: 10, color: '#333', fontWeight: '700', letterSpacing: 1 },
  histPair: { gap: 3 },
  histUser: { fontSize: 12, color: '#6060a0' },
  histBot:  { fontSize: 13, color: '#a0a0c0', lineHeight: 18 },
});
