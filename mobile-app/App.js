// App.js — Project Cortex mobile app
import React from 'react';
import { NavigationContainer } from '@react-navigation/native';
import { createBottomTabNavigator } from '@react-navigation/bottom-tabs';
import { Text, Platform } from 'react-native';
import { StatusBar } from 'expo-status-bar';

import KidScreen     from './src/screens/KidScreen';
import ParentScreen  from './src/screens/ParentScreen';
import SettingsScreen from './src/screens/SettingsScreen';

const Tab = createBottomTabNavigator();

export default function App() {
  return (
    <NavigationContainer>
      <StatusBar style="light" />
      <Tab.Navigator
        screenOptions={{
          headerShown: false,
          tabBarStyle: {
            backgroundColor: '#0f0f13',
            borderTopColor: '#1a1a24',
            borderTopWidth: 1,
            paddingBottom: Platform.OS === 'ios' ? 20 : 8,
            paddingTop: 8,
            height: Platform.OS === 'ios' ? 80 : 60,
          },
          tabBarActiveTintColor:   '#8b5cf6',
          tabBarInactiveTintColor: '#333',
          tabBarLabelStyle: { fontSize: 11, fontWeight: '700' },
        }}
      >
        <Tab.Screen
          name="Talk"
          component={KidScreen}
          options={{
            tabBarLabel: 'Talk',
            tabBarIcon: ({ color }) => <Text style={{ fontSize: 22, color }}>🎤</Text>,
          }}
        />
        <Tab.Screen
          name="Dashboard"
          component={ParentScreen}
          options={{
            tabBarLabel: 'Dashboard',
            tabBarIcon: ({ color }) => <Text style={{ fontSize: 22, color }}>🤖</Text>,
          }}
        />
        <Tab.Screen
          name="Settings"
          component={SettingsScreen}
          options={{
            tabBarLabel: 'Settings',
            tabBarIcon: ({ color }) => <Text style={{ fontSize: 22, color }}>⚙️</Text>,
          }}
        />
      </Tab.Navigator>
    </NavigationContainer>
  );
}
