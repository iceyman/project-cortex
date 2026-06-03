import React, { useState, useEffect, useCallback } from 'react';
import { View, Text, StyleSheet, ScrollView, Pressable, TextInput, RefreshControl, Switch, Alert } from 'react-native';
import { fetchHealth, dashboardSay, setMode, setVolume, forceSleep, forceWake, forceOtaUpdate, forceReboot, resetMemory, getRecentHistory } from '../cerebroApi';
import useRobotStatus from '../hooks/useRobotStatus';

var ROBOTS = [
  { id: 'robot1', name: 'Kira', color: '#8b5cf6' },
  { id: 'robot2', name: 'Rumi', color: '#ff6eb4' },
];

export default function ParentScreen() {
  var _h = useState(null); var health = _h[0]; var setHealth = _h[1];
  var _hist = useState({}); var histories = _hist[0]; var setHistories = _hist[1];
  var _say = useState({ robot1: '', robot2: '' }); var sayText = _say[0]; var setSayText = _say[1];
  var _ref = useState(false); var refreshing = _ref[0]; var setRefreshing = _ref[1];
  var _t = useState(''); var toast = _t[0]; var setToast = _t[1];
  var robotStatus = useRobotStatus(15000);

  function showToast(msg) { setToast(msg); setTimeout(function() { setToast(''); }, 3000); }

  var load = useCallback(function() {
    fetchHealth().then(function(h) { setHealth(h); }).catch(function() { showToast('Cannot reach Cerebro'); });
    ROBOTS.forEach(function(r) {
      getRecentHistory(r.id, 3).then(function(d) {
        setHistories(function(prev) { var n = {}; n[r.id] = d.history || []; return { ...prev, ...n }; });
      }).catch(function() {});
    });
  }, []);

  useEffect(function() { load(); }, [load]);

  function onRefresh() { setRefreshing(true); load(); setTimeout(function() { setRefreshing(false); }, 1000); }

  function handleSay(id) {
    var text = (sayText[id] || '').trim();
    if (!text) return;
    dashboardSay(id, text).then(function() { showToast('Message queued!'); });
    setSayText(function(prev) { var n = {}; n[id] = ''; return { ...prev, ...n }; });
  }

  var statusColor = robotStatus.cerebroOnline ? '#4caf50' : '#ef4444';
  var statusText = robotStatus.cerebroOnline ? 'Cerebro Online' : 'Cerebro Offline';

  return (
    <View style={styles.container}>
      {toast ? <View style={styles.toast}><Text style={styles.toastText}>{toast}</Text></View> : null}
      <ScrollView style={{ flex: 1, width: '100%' }}
        refreshControl={<RefreshControl refreshing={refreshing} onRefresh={onRefresh} tintColor="#8b5cf6" />}
        contentContainerStyle={{ padding: 16, gap: 14 }}>

        <View style={styles.sbar}>
          <View style={[styles.sDot, { backgroundColor: statusColor }]} />
          <Text style={[styles.sText, { color: statusColor }]}>{statusText}</Text>
          {health && <Text style={styles.sModel}>{"Model: " + (health.model || '?')}</Text>}
        </View>

        <View style={styles.gbtns}>
          <Pressable style={styles.gbtn} onPress={function() { forceSleep().then(function() { showToast('Sleep sent'); }); }}>
            <Text style={styles.gbtnText}>{"Sleep All"}</Text>
          </Pressable>
          <Pressable style={styles.gbtn} onPress={function() { forceWake().then(function() { showToast('Wake sent'); }); }}>
            <Text style={styles.gbtnText}>{"Wake All"}</Text>
          </Pressable>
        </View>

        {ROBOTS.map(function(robot) {
          var hist = histories[robot.id] || [];
          return (
            <View key={robot.id} style={[styles.card, { borderColor: robot.color + '44' }]}>
              <View style={styles.cardHead}>
                <Text style={[styles.robotName, { color: robot.color }]}>{robot.name}</Text>
                <View style={[styles.onlineBadge, { borderColor: '#4caf5044', backgroundColor: '#1a3a1a' }]}>
                  <Text style={{ color: '#4caf50', fontSize: 11, fontWeight: '700' }}>{"Online"}</Text>
                </View>
              </View>

              <View style={styles.qrow}>
                {[['Hey!', 'Hey! How is everyone?'], ['Dance', 'dance party'], ['Joke', 'Tell me a joke'], ['Night', 'Goodnight!'], ['Weather', 'What is the weather?']].map(function(pair) {
                  return (
                    <Pressable key={pair[0]} style={styles.qbtn} onPress={function() { dashboardSay(robot.id, pair[1]).then(function() { showToast('Sent!'); }); }}>
                      <Text style={styles.qbtnText}>{pair[0]}</Text>
                    </Pressable>
                  );
                })}
              </View>

              <TextInput style={styles.input} placeholder={"Type something for " + robot.name + "..."}
                placeholderTextColor="#444" value={sayText[robot.id]}
                onChangeText={function(t) { setSayText(function(prev) { var n = {}; n[robot.id] = t; return { ...prev, ...n }; }); }}
                onSubmitEditing={function() { handleSay(robot.id); }} returnKeyType="send" />

              <View style={styles.btnRow}>
                <Pressable style={styles.sayBtn} onPress={function() { handleSay(robot.id); }}>
                  <Text style={styles.sayBtnText}>{"Say it"}</Text>
                </Pressable>
                <Pressable style={styles.rstBtn} onPress={function() {
                  Alert.alert('Reset', 'Clear ' + robot.name + ' memory?', [
                    { text: 'Cancel', style: 'cancel' },
                    { text: 'Reset', style: 'destructive', onPress: function() { resetMemory(robot.id).then(function() { showToast('Cleared'); load(); }); } },
                  ]);
                }}>
                  <Text style={styles.rstBtnText}>{"Reset"}</Text>
                </Pressable>
                <Pressable style={styles.updBtn} onPress={function() { forceReboot(robot.id).then(function() { showToast('Reboot queued'); }); }}>
                  <Text style={styles.updBtnText}>{"Reboot"}</Text>
                </Pressable>
              </View>

              {hist.length > 0 && (
                <View style={styles.histBox}>
                  <Text style={styles.histTitle}>{"RECENT"}</Text>
                  {hist.map(function(h, i) {
                    return (
                      <View key={i} style={styles.histPair}>
                        <Text style={styles.histU}>{h.user}</Text>
                        <Text style={[styles.histA, { color: robot.color }]}>{h.assistant}</Text>
                      </View>
                    );
                  })}
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

var styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#0f0f13', alignItems: 'center' },
  toast: { position: 'absolute', top: 10, left: 20, right: 20, zIndex: 999, backgroundColor: '#1a3a1a', borderRadius: 12, padding: 12, borderWidth: 1, borderColor: '#4caf5088' },
  toastText: { color: '#90e0a0', fontWeight: '700', textAlign: 'center' },
  sbar: { backgroundColor: '#1a1a24', borderRadius: 12, padding: 12, flexDirection: 'row', alignItems: 'center', gap: 8, borderWidth: 1, borderColor: '#252535' },
  sDot: { width: 8, height: 8, borderRadius: 4 },
  sText: { fontWeight: '700', fontSize: 13 },
  sModel: { color: '#555', fontSize: 11, marginLeft: 'auto' },
  gbtns: { flexDirection: 'row', gap: 10 },
  gbtn: { flex: 1, padding: 12, borderRadius: 10, alignItems: 'center', backgroundColor: '#1e1e2e', borderWidth: 1, borderColor: '#2a2a40' },
  gbtnText: { color: '#a0a0c0', fontWeight: '700', fontSize: 13 },
  card: { backgroundColor: '#1a1a24', borderRadius: 16, padding: 16, borderWidth: 1, gap: 10 },
  cardHead: { flexDirection: 'row', alignItems: 'center', justifyContent: 'space-between' },
  robotName: { fontSize: 22, fontWeight: '800' },
  onlineBadge: { flexDirection: 'row', alignItems: 'center', gap: 5, paddingHorizontal: 8, paddingVertical: 4, borderRadius: 12, borderWidth: 1 },
  qrow: { flexDirection: 'row', gap: 6, flexWrap: 'wrap' },
  qbtn: { paddingHorizontal: 10, paddingVertical: 8, borderRadius: 8, backgroundColor: '#1c1c2e', borderWidth: 1, borderColor: '#2a2a42' },
  qbtnText: { color: '#9090c0', fontSize: 12, fontWeight: '700' },
  input: { backgroundColor: '#111118', borderWidth: 1, borderColor: '#222232', borderRadius: 10, padding: 10, color: '#e8e8f0', fontSize: 14 },
  btnRow: { flexDirection: 'row', gap: 8 },
  sayBtn: { flex: 2, backgroundColor: '#2a4060', borderRadius: 9, padding: 10, alignItems: 'center' },
  sayBtnText: { color: '#80b8f0', fontWeight: '700', fontSize: 13 },
  rstBtn: { flex: 1, backgroundColor: '#3a1818', borderRadius: 9, padding: 10, alignItems: 'center' },
  rstBtnText: { color: '#c07070', fontWeight: '700', fontSize: 13 },
  updBtn: { flex: 1, backgroundColor: '#1a1a2a', borderRadius: 9, padding: 10, alignItems: 'center' },
  updBtnText: { color: '#7090d0', fontWeight: '700', fontSize: 13 },
  histBox: { backgroundColor: '#111118', borderRadius: 10, padding: 10, gap: 8 },
  histTitle: { fontSize: 10, color: '#333', fontWeight: '700', letterSpacing: 1 },
  histPair: { gap: 3 },
  histU: { fontSize: 12, color: '#6060a0' },
  histA: { fontSize: 13, lineHeight: 18 },
});
