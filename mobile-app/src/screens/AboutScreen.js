import React from 'react';
import { View, Text, StyleSheet, ScrollView } from 'react-native';

export default function AboutScreen() {
  return (
    <ScrollView style={styles.container} contentContainerStyle={styles.content}>
      <Text style={styles.title}>{"Project Cortex"}</Text>
      <Text style={styles.version}>{"AI Desktop Robot Crew"}</Text>

      <View style={styles.card}>
        <Text style={styles.cardTitle}>{"THE CREW"}</Text>
        <Text style={styles.cardText}>{"Kira - Main host. Warm, enthusiastic, gaming nerd."}</Text>
        <Text style={styles.cardText}>{"Rumi - Kid companion. Gentle, playful, always excited."}</Text>
      </View>

      <View style={styles.card}>
        <Text style={styles.cardTitle}>{"HARDWARE"}</Text>
        <Text style={styles.cardText}>{"M5Stack CoreS3 SE (x2)"}</Text>
        <Text style={styles.cardText}>{"StackChan BSP servos"}</Text>
        <Text style={styles.cardText}>{"Cerebro server (Ubuntu + RTX 2080 Ti)"}</Text>
      </View>

      <View style={styles.card}>
        <Text style={styles.cardTitle}>{"STACK"}</Text>
        <Text style={styles.cardText}>{"Ollama + qwen3:8b - local LLM"}</Text>
        <Text style={styles.cardText}>{"Whisper - speech recognition"}</Text>
        <Text style={styles.cardText}>{"gTTS - text to speech"}</Text>
        <Text style={styles.cardText}>{"Flask - Cerebro server"}</Text>
      </View>

      <View style={styles.claudeCard}>
        <Text style={styles.claudeText}>{"Built with help from Claude - who also helped wrangle the Avatar and StackChan-BSP libraries into submission."}</Text>
      </View>

      <Text style={styles.ver}>{"v1.0.0"}</Text>
    </ScrollView>
  );
}

var styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#0f0f13' },
  content: { padding: 20, gap: 16 },
  title: { fontSize: 28, fontWeight: '900', color: 'white', textAlign: 'center' },
  version: { fontSize: 14, color: '#555', textAlign: 'center', marginTop: -10 },
  card: { backgroundColor: '#1a1a24', borderRadius: 14, padding: 16, borderWidth: 1, borderColor: '#252535', gap: 6 },
  cardTitle: { fontSize: 12, color: '#555', fontWeight: '700', letterSpacing: 1, marginBottom: 4 },
  cardText: { fontSize: 14, color: '#a0a0c0', lineHeight: 22 },
  claudeCard: { backgroundColor: '#1a1030', borderRadius: 14, padding: 16, borderWidth: 1, borderColor: '#8b5cf644' },
  claudeText: { fontSize: 14, color: '#c4b5fd', lineHeight: 22, fontStyle: 'italic', textAlign: 'center' },
  ver: { fontSize: 11, color: '#333', textAlign: 'center', marginTop: 8 },
});
