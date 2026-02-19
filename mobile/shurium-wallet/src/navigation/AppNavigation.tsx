/**
 * SHURIUM Mobile Wallet - Main Navigation
 * App navigation configuration
 */

import React from 'react';
import { NavigationContainer, DefaultTheme } from '@react-navigation/native';
import { createNativeStackNavigator } from '@react-navigation/native-stack';
import { createBottomTabNavigator } from '@react-navigation/bottom-tabs';
import { View, Text, StyleSheet } from 'react-native';

// Import screens
import { HomeScreen } from '../screens/HomeScreen';
import { SendScreen } from '../screens/SendScreen';
// These will be implemented later
// import { ReceiveScreen } from '../screens/ReceiveScreen';
// import { StakingScreen } from '../screens/StakingScreen';
// import { UBIScreen } from '../screens/UBIScreen';
// import { SettingsScreen } from '../screens/SettingsScreen';
// import { TransactionsScreen } from '../screens/TransactionsScreen';

// Placeholder screens for now
const PlaceholderScreen: React.FC<{ name: string }> = ({ name }) => (
  <View style={styles.placeholder}>
    <Text style={styles.placeholderText}>{name} Screen</Text>
    <Text style={styles.placeholderSubtext}>Coming soon</Text>
  </View>
);

const ReceiveScreen = () => <PlaceholderScreen name="Receive" />;
const StakingScreen = () => <PlaceholderScreen name="Staking" />;
const UBIScreen = () => <PlaceholderScreen name="UBI" />;
const SettingsScreen = () => <PlaceholderScreen name="Settings" />;
const TransactionsScreen = () => <PlaceholderScreen name="Transactions" />;
const FaucetScreen = () => <PlaceholderScreen name="Faucet" />;
const TransactionDetailScreen = () => <PlaceholderScreen name="Transaction Detail" />;

// Navigation types
export type RootStackParamList = {
  MainTabs: undefined;
  Send: undefined;
  Receive: undefined;
  Staking: undefined;
  UBI: undefined;
  Transactions: undefined;
  TransactionDetail: { txid: string };
  Faucet: undefined;
  Settings: undefined;
  QRScanner: { onScan: (data: string) => void };
};

export type MainTabsParamList = {
  Home: undefined;
  Wallet: undefined;
  Stake: undefined;
  Settings: undefined;
};

const Stack = createNativeStackNavigator<RootStackParamList>();
const Tab = createBottomTabNavigator<MainTabsParamList>();

// Dark theme
const DarkTheme = {
  ...DefaultTheme,
  colors: {
    ...DefaultTheme.colors,
    primary: '#2196F3',
    background: '#121212',
    card: '#1E1E1E',
    text: '#FFFFFF',
    border: '#333333',
    notification: '#F44336',
  },
};

// Tab bar icon component
const TabIcon: React.FC<{ name: string; focused: boolean }> = ({ name, focused }) => {
  const getIcon = () => {
    switch (name) {
      case 'Home': return 'H';
      case 'Wallet': return 'W';
      case 'Stake': return 'S';
      case 'Settings': return 'G';
      default: return '?';
    }
  };

  return (
    <View style={[styles.tabIcon, focused && styles.tabIconFocused]}>
      <Text style={[styles.tabIconText, focused && styles.tabIconTextFocused]}>
        {getIcon()}
      </Text>
    </View>
  );
};

// Main tab navigator
const MainTabs: React.FC = () => {
  return (
    <Tab.Navigator
      screenOptions={({ route }) => ({
        tabBarIcon: ({ focused }) => <TabIcon name={route.name} focused={focused} />,
        tabBarActiveTintColor: '#2196F3',
        tabBarInactiveTintColor: '#666',
        tabBarStyle: {
          backgroundColor: '#1E1E1E',
          borderTopColor: '#333',
          paddingBottom: 8,
          paddingTop: 8,
          height: 60,
        },
        headerStyle: {
          backgroundColor: '#1E1E1E',
        },
        headerTintColor: '#fff',
        headerTitleStyle: {
          fontWeight: 'bold',
        },
      })}
    >
      <Tab.Screen
        name="Home"
        component={HomeScreen}
        options={{
          title: 'SHURIUM',
          headerTitleAlign: 'center',
        }}
      />
      <Tab.Screen
        name="Wallet"
        component={TransactionsScreen}
        options={{
          title: 'Transactions',
        }}
      />
      <Tab.Screen
        name="Stake"
        component={StakingScreen}
        options={{
          title: 'Staking',
        }}
      />
      <Tab.Screen
        name="Settings"
        component={SettingsScreen}
        options={{
          title: 'Settings',
        }}
      />
    </Tab.Navigator>
  );
};

// Main navigation
export const AppNavigation: React.FC = () => {
  return (
    <NavigationContainer theme={DarkTheme}>
      <Stack.Navigator
        screenOptions={{
          headerStyle: {
            backgroundColor: '#1E1E1E',
          },
          headerTintColor: '#fff',
          headerTitleStyle: {
            fontWeight: 'bold',
          },
          headerBackTitleVisible: false,
        }}
      >
        <Stack.Screen
          name="MainTabs"
          component={MainTabs}
          options={{ headerShown: false }}
        />
        <Stack.Screen
          name="Send"
          component={SendScreen}
          options={{ title: 'Send SHR' }}
        />
        <Stack.Screen
          name="Receive"
          component={ReceiveScreen}
          options={{ title: 'Receive SHR' }}
        />
        <Stack.Screen
          name="Staking"
          component={StakingScreen}
          options={{ title: 'Staking' }}
        />
        <Stack.Screen
          name="UBI"
          component={UBIScreen}
          options={{ title: 'Universal Basic Income' }}
        />
        <Stack.Screen
          name="Transactions"
          component={TransactionsScreen}
          options={{ title: 'Transaction History' }}
        />
        <Stack.Screen
          name="TransactionDetail"
          component={TransactionDetailScreen}
          options={{ title: 'Transaction' }}
        />
        <Stack.Screen
          name="Faucet"
          component={FaucetScreen}
          options={{ title: 'Test Faucet' }}
        />
        <Stack.Screen
          name="Settings"
          component={SettingsScreen}
          options={{ title: 'Settings' }}
        />
      </Stack.Navigator>
    </NavigationContainer>
  );
};

const styles = StyleSheet.create({
  placeholder: {
    flex: 1,
    justifyContent: 'center',
    alignItems: 'center',
    backgroundColor: '#121212',
  },
  placeholderText: {
    color: '#fff',
    fontSize: 24,
    fontWeight: 'bold',
  },
  placeholderSubtext: {
    color: '#666',
    fontSize: 16,
    marginTop: 8,
  },
  tabIcon: {
    width: 32,
    height: 32,
    borderRadius: 16,
    justifyContent: 'center',
    alignItems: 'center',
    backgroundColor: 'transparent',
  },
  tabIconFocused: {
    backgroundColor: 'rgba(33, 150, 243, 0.2)',
  },
  tabIconText: {
    color: '#666',
    fontSize: 16,
    fontWeight: 'bold',
  },
  tabIconTextFocused: {
    color: '#2196F3',
  },
});

export default AppNavigation;
