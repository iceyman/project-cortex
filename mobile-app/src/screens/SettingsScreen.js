import React, { useState, useEffect } from 'react';
import { View, Text, StyleSheet, TextInput, Pressable } from 'react-native';
import AsyncStorage from '@react-native-async-storage/async-storage';
import { fetchHealth, saveCerebroUrl } from '../cerebroApi';

export default function SettingsScreen() {
  var _ip = useState('YOUR_CEREBRO_IP'); var ip = _ip[0]; var setIp = _ip[1];
  var _port = useState('5005'); var port = _port[0]; var setPort = _port[1];
  var _kid = useState('YOUR_KIDS_NAME'); var kidName = _kid[0]; var setKidName = _kid[1];
  var _robot = useState('robot2'); var myRobot = _robot[0]; var setMyRobot = _robot[1];
  var _st = useState(''); var status = _st[0]; var setStatus = _st[1];

  useEffect(function() {
    AsyncStorage.getItem('cerebro_ip').then(function(v) { if (v) setIp(v); });
    AsyncStorage.getItem('cerebro_port').then(function(v) { if (v) setPort(v); });
    AsyncStorage.getItem('kid_name').then(function(v) { if (v) setKidName(v); });
    AsyncStorage.getItem('my_robot').then(function(v) { if (v) setMyRobot(v); });
  }, []);

  function save() {
    saveCerebroUrl(ip.trim(), port.trim());
    AsyncStorage.setItem('kid_name', kidName.trim());
    setStatus('Saved!');
    setTimeout(function() { setStatus(''); }, 2500);
  }

  function test() {
    setStatus('Testing...');
    fetchHealth()
      .then(function(h) { setStatus('Connected! Robots: ' + (h.robots || []).join(', ')); })
      .catch(function() { setStatus('Cannot reach Cerebro'); });
  }

  function pickRobot(id) {
    setMyRobot(id);
    AsyncStorage.setItem('my_robot', id);
  }

  return (
    <View style={styles.container}>
      <Text style={styles.title}>{"Settings"}</Text>

      <View style={styles.section}>
        <Text style={styles.label}>{"My Robot"}</Text>
        <View style={styles.robotRow}>
          {[['robot2', 'Rumi', '#ff6eb4'], ['robot1', 'Kira', '#8b5cf6']].map(function(r) {
            var isSel = myRobot === r[0];
            return (
              <Pressable key={r[0]} style={[styles.robotBtn, isSel && { borderColor: r[2], backgroundColor: r[2] + '22' }]}
                onPress={function() { pickRobot(r[0]); }}>
                <Text style={[styles.robotBtnText, isSel && { color: r[2], fontWeight: '800' }]}>{r[1]}</Text>
              </Pressable>
            );
          })}
        </View>
      </View>

      <View style={styles.section}>
        <Text style={styles.label}>{"Cerebro IP"}</Text>
        <TextInput style={styles.input} value={ip} onChangeText={setIp} placeholder="YOUR_CEREBRO_IP" placeholderTextColor="#333" keyboardType="numeric" autoCapitalize="none" />
      </View>

      <View style={styles.section}>
        <Text style={styles.label}>{"Port"}</Text>
        <TextInput style={styles.input} value={port} onChangeText={setPort} placeholder="5005" placeholderTextColor="#333" keyboardType="numeric" />
      </View>

      <View style={styles.section}>
        <Text style={styles.label}>{"Kid's Name"}</Text>
        <TextInput style={styles.input} value={kidName} onChangeText={setKidName} placeholder="YOUR_KIDS_NAME" placeholderTextColor="#333" autoCapitalize="words" />
      </View>

      {status ? <Text style={styles.status}>{status}</Text> : null}

      <Pressable style={styles.testBtn} onPress={test}>
        <Text style={styles.testBtnText}>{"Test Connection"}</Text>
      </Pressable>

      <Pressable style={styles.saveBtn} onPress={save}>
        <Text style={styles.saveBtnText}>{"Save Settings"}</Text>
      </Pressable>

      <View style={styles.infoBox}>
        <Text style={styles.infoTitle}>{"Remote Access"}</Text>
        <Text style={styles.infoText}>{"Use your Tailscale IP to access robots from anywhere. Install Tailscale on this device and Cerebro, log in with the same account."}</Text>
      </View>
    </View>
  );
}

var styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#0f0f13', padding: 20, gap: 12 },
  title: { fontSize: 24, fontWeight: '800', color: 'white', marginBottom: 4 },
  section: { gap: 6 },
  label: { color: '#777', fontSize: 14, fontWeight: '600' },
  input: { backgroundColor: '#1a1a24', borderWidth: 1, borderColor: '#252535', borderRadius: 10, padding: 12, color: '#e8e8f0', fontSize: 15 },
  robotRow: { flexDirection: 'row', gap: 10 },
  robotBtn: { flex: 1, padding: 12, borderRadius: 12, borderWidth: 2, borderColor: '#252535', backgroundColor: '#1a1a24', alignItems: 'center' },
  robotBtnText: { fontSize: 14, fontWeight: '600', color: '#555' },
  status: { color: '#90e0a0', fontWeight: '700', textAlign: 'center', padding: 8 },
  testBtn: { backgroundColor: '#1a2a3a', borderRadius: 12, padding: 14, alignItems: 'center', borderWidth: 1, borderColor: '#2a4060' },
  testBtnText: { color: '#80b8f0', fontWeight: '700', fontSize: 15 },
  saveBtn: { backgroundColor: '#2a1a4a', borderRadius: 12, padding: 14, alignItems: 'center' },
  saveBtnText: { color: '#c4b5fd', fontWeight: '700', fontSize: 15 },
  infoBox: { backgroundColor: '#1a1a24', borderRadius: 12, padding: 14, borderWidth: 1, borderColor: '#252535', gap: 6, marginTop: 8 },
  infoTitle: { color: '#8b5cf6', fontWeight: '700', fontSize: 13 },
  infoText: { color: '#555', fontSize: 12, lineHeight: 18 },
});
