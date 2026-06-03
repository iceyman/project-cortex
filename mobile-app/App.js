import React, { useState, useEffect, useRef } from 'react';
import { NavigationContainer } from '@react-navigation/native';
import { createBottomTabNavigator } from '@react-navigation/bottom-tabs';
import { Text, Platform, View, ActivityIndicator } from 'react-native';
import { StatusBar } from 'expo-status-bar';
import AsyncStorage from '@react-native-async-storage/async-storage';

import KidScreen from './src/screens/KidScreen';
import ParentScreen from './src/screens/ParentScreen';
import SettingsScreen from './src/screens/SettingsScreen';
import AboutScreen from './src/screens/AboutScreen';
import ChooseRobotScreen from './src/screens/ChooseRobotScreen';
import PinLock from './src/components/PinLock';

var Tab = createBottomTabNavigator();
function LockedScreen() { return null; }

export default function App() {
  var _l = useState(true); var loading = _l[0]; var setLoading = _l[1];
  var _r = useState(null); var myRobot = _r[0]; var setMyRobot = _r[1];
  var _pv = useState(false); var pinVisible = _pv[0]; var setPinVisible = _pv[1];
  var _pu = useState(false); var pinUnlocked = _pu[0]; var setPinUnlocked = _pu[1];
  var _pt = useState(null); var pendingTab = _pt[0]; var setPendingTab = _pt[1];
  var lockTimer = useRef(null);

  useEffect(function() {
    AsyncStorage.getItem('my_robot').then(function(r) {
      setMyRobot(r || null);
      setLoading(false);
    });
  }, []);

  function requirePin(tabName, navigation) {
    if (pinUnlocked) { navigation.navigate(tabName); return; }
    setPendingTab({ tabName: tabName, navigation: navigation });
    setPinVisible(true);
  }

  function onUnlock() {
    setPinVisible(false);
    setPinUnlocked(true);
    if (pendingTab) { pendingTab.navigation.navigate(pendingTab.tabName); setPendingTab(null); }
    if (lockTimer.current) clearTimeout(lockTimer.current);
    lockTimer.current = setTimeout(function() { setPinUnlocked(false); }, 5 * 60 * 1000);
  }

  if (loading) {
    return (
      <View style={{ flex: 1, backgroundColor: '#0f0f13', alignItems: 'center', justifyContent: 'center' }}>
        <ActivityIndicator color="#8b5cf6" size="large" />
      </View>
    );
  }

  if (!myRobot) {
    return (
      <View style={{ flex: 1 }}>
        <StatusBar style="light" />
        <ChooseRobotScreen onSelect={setMyRobot} />
      </View>
    );
  }

  var robotColor = myRobot === 'robot2' ? '#ff6eb4' : '#8b5cf6';
  var robotName = myRobot === 'robot2' ? 'Rumi' : 'Kira';

  return (
    <NavigationContainer>
      <StatusBar style="light" />
      <PinLock visible={pinVisible} onUnlock={onUnlock}
        onCancel={function() { setPinVisible(false); setPendingTab(null); }} />
      <Tab.Navigator screenOptions={{
        headerShown: false,
        tabBarStyle: { backgroundColor: '#0f0f13', borderTopColor: '#1a1a24', borderTopWidth: 1, paddingBottom: Platform.OS === 'ios' ? 20 : 8, paddingTop: 8, height: Platform.OS === 'ios' ? 80 : 60 },
        tabBarActiveTintColor: robotColor,
        tabBarInactiveTintColor: '#333',
        tabBarLabelStyle: { fontSize: 11, fontWeight: '700' },
      }}>
        <Tab.Screen name="Talk" options={{
          tabBarLabel: 'Talk to ' + robotName,
          tabBarIcon: function(p) { return <Text style={{ fontSize: 22, color: p.color }}>{"\uD83C\uDFA4"}</Text>; },
        }}>
          {function() { return <KidScreen myRobot={myRobot} />; }}
        </Tab.Screen>
        <Tab.Screen name="Dashboard"
          component={pinUnlocked ? ParentScreen : LockedScreen}
          listeners={function(p) { return { tabPress: function(e) { if (!pinUnlocked) { e.preventDefault(); requirePin('Dashboard', p.navigation); } } }; }}
          options={{
            tabBarLabel: 'Dashboard',
            tabBarIcon: function(p) { return <Text style={{ fontSize: 22, color: p.color }}>{pinUnlocked ? "\uD83E\uDD16" : "\uD83D\uDD12"}</Text>; },
          }} />
        <Tab.Screen name="Settings"
          component={pinUnlocked ? SettingsScreen : LockedScreen}
          listeners={function(p) { return { tabPress: function(e) { if (!pinUnlocked) { e.preventDefault(); requirePin('Settings', p.navigation); } } }; }}
          options={{
            tabBarLabel: 'Settings',
            tabBarIcon: function(p) { return <Text style={{ fontSize: 22, color: p.color }}>{pinUnlocked ? "\u2699\uFE0F" : "\uD83D\uDD12"}</Text>; },
          }} />
        <Tab.Screen name="About" component={AboutScreen} options={{
          tabBarLabel: 'About',
          tabBarIcon: function(p) { return <Text style={{ fontSize: 22, color: p.color }}>{"\u2139\uFE0F"}</Text>; },
        }} />
      </Tab.Navigator>
    </NavigationContainer>
  );
}
