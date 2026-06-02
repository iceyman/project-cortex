// src/screens/SettingsScreen.js
import React, { useState, useEffect } from 'react';
import { View, Text, StyleSheet, TextInput, Pressable, Alert } from 'react-native';
import AsyncStorage from '@react-native-async-storage/async-storage';
import { fetchHealth, saveCerebroUrl } from '../cerebroApi';

export default function SettingsScreen() {
  const [ip,      setIp]      = useState('YOUR_CEREBRO_IP');
  const [port,    setPort]    = useState('5005');
  const [kidName, setKidName] = useState('YOUR_SONS_NAME');
  const [status,  setStatus]  = useState('');
  const [testing, setTesting] = useState(false);

  useEffect(() => {
    AsyncStorage.getItem('cerebro_ip').then(v => v && setIp(v));
    AsyncStorage.getItem('cerebro_port').then(v => v && setPort(v));
    AsyncStorage.getItem('kid_name').then(v => v && setKidName(v));
  }, []);

  const save = async () => {
    await saveCerebroUrl(ip.trim(), port.trim());
    await AsyncStorage.setItem('kid_name', kidName.trim());
    setStatus('✅ Saved!');
    setTimeout(() => setStatus(''), 2500);
  };

  const test = async () => {
    setTesting(true);
    setStatus('Testing connection...');
    try {
      const h = await fetchHealth();
      setStatus(`✅ Connected! Robots: ${h.robots?.join(', ')}`);
    } catch {
      setStatus('❌ Cannot reach Cerebro — check IP and that the server is running');
    }
    setTesting(false);
  };

  return (
    <View style={styles.container}>
      <Text style={styles.title}>⚙️ Settings</Text>

      <View style={styles.section}>
        <Text style={styles.label}>Cerebro IP Address</Text>
        <TextInput
          style={styles.input}
          value={ip}
          onChangeText={setIp}
          placeholder="YOUR_CEREBRO_IP"
          placeholderTextColor="#333"
          keyboardType="numeric"
          autoCapitalize="none"
        />
        <Text style={styles.hint}>Find this in your router or run 'hostname -I' on Cerebro</Text>
      </View>

      <View style={styles.section}>
        <Text style={styles.label}>Port</Text>
        <TextInput
          style={styles.input}
          value={port}
          onChangeText={setPort}
          placeholder="5005"
          placeholderTextColor="#333"
          keyboardType="numeric"
        />
      </View>

      <View style={styles.section}>
        <Text style={styles.label}>Kid's Name (for Rumi's kid mode)</Text>
        <TextInput
          style={styles.input}
          value={kidName}
          onChangeText={setKidName}
          placeholder="Your kid's name"
          placeholderTextColor="#333"
          autoCapitalize="words"
        />
      </View>

      {status ? <Text style={styles.status}>{status}</Text> : null}

      <Pressable style={styles.testBtn} onPress={test} disabled={testing}>
        <Text style={styles.testBtnText}>🔌 Test Connection</Text>
      </Pressable>

      <Pressable style={styles.saveBtn} onPress={save}>
        <Text style={styles.saveBtnText}>💾 Save Settings</Text>
      </Pressable>

      <View style={styles.infoBox}>
        <Text style={styles.infoTitle}>📡 Remote Access (Away from Home)</Text>
        <Text style={styles.infoText}>
          To use the app outside your home WiFi, you need to set up port forwarding on your router (port 5005 → Cerebro's IP) or use a VPN like Tailscale. Tailscale is free and easy — install on Cerebro and your phone, then use the Tailscale IP here.
        </Text>
      </View>
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1, backgroundColor: '#0f0f13', padding: 20, gap: 12,
  },
  title: { fontSize: 24, fontWeight: '800', color: 'white', marginBottom: 4 },
  section: { gap: 6 },
  label: { color: '#777', fontSize: 14, fontWeight: '600' },
  hint:  { color: '#444', fontSize: 12 },
  input: {
    backgroundColor: '#1a1a24', borderWidth: 1, borderColor: '#252535',
    borderRadius: 10, padding: 12, color: '#e8e8f0', fontSize: 15,
  },
  status: { color: '#90e0a0', fontWeight: '700', textAlign: 'center', padding: 8 },
  testBtn: {
    backgroundColor: '#1a2a3a', borderRadius: 12, padding: 14,
    alignItems: 'center', borderWidth: 1, borderColor: '#2a4060',
  },
  testBtnText: { color: '#80b8f0', fontWeight: '700', fontSize: 15 },
  saveBtn: {
    backgroundColor: '#2a1a4a', borderRadius: 12, padding: 14,
    alignItems: 'center',
  },
  saveBtnText: { color: '#c4b5fd', fontWeight: '700', fontSize: 15 },
  infoBox: {
    backgroundColor: '#1a1a24', borderRadius: 12, padding: 14,
    borderWidth: 1, borderColor: '#252535', gap: 6, marginTop: 8,
  },
  infoTitle: { color: '#8b5cf6', fontWeight: '700', fontSize: 13 },
  infoText:  { color: '#555', fontSize: 12, lineHeight: 18 },
});
