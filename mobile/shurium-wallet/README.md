# SHURIUM Mobile Wallet

A React Native mobile wallet application for the SHURIUM cryptocurrency.

## Features

- **Multi-Network Support**: Connect to mainnet, testnet, or regtest
- **Secure Key Storage**: BIP39 mnemonic with secure storage
- **Transaction Management**: Send and receive SHR
- **Staking**: Delegate to validators and earn rewards
- **UBI Claims**: Claim Universal Basic Income payments
- **Real-time Updates**: Live balance and transaction updates
- **QR Code Support**: Scan and generate payment QR codes

## Getting Started

### Prerequisites

- Node.js 18+
- npm or yarn
- React Native CLI
- Xcode (for iOS)
- Android Studio (for Android)
- A running SHURIUM node (for RPC connectivity)

### Installation

1. Install dependencies:
   ```bash
   npm install
   ```

2. Install iOS pods (macOS only):
   ```bash
   cd ios && pod install && cd ..
   ```

3. Start the development server:
   ```bash
   npm start
   ```

4. Run on your device:
   ```bash
   # iOS
   npm run ios
   
   # Android
   npm run android
   ```

### Connecting to a Node

By default, the wallet connects to `localhost` on the standard SHURIUM ports:
- Mainnet: 8332
- Testnet: 18332
- Regtest: 18443

To connect to a remote node, update the RPC configuration in the app settings.

## Project Structure

```
shurium-wallet/
├── App.tsx                 # Main application entry
├── index.js                # React Native entry point
├── src/
│   ├── components/         # Reusable UI components
│   ├── hooks/              # Custom React hooks
│   ├── navigation/         # Navigation configuration
│   │   └── AppNavigation.tsx
│   ├── screens/            # App screens
│   │   ├── HomeScreen.tsx
│   │   └── SendScreen.tsx
│   ├── services/           # Backend services
│   │   └── rpc.ts          # RPC client for SHURIUM node
│   ├── store/              # State management
│   │   └── wallet.ts       # Zustand wallet store
│   ├── types/              # TypeScript type definitions
│   │   └── index.ts
│   └── utils/              # Utility functions
│       └── crypto.ts       # Cryptographic utilities
├── package.json
└── tsconfig.json
```

## Key Components

### RPC Service (`src/services/rpc.ts`)

Handles all communication with the SHURIUM node via JSON-RPC:
- Blockchain info queries
- Balance and transaction fetching
- Transaction sending
- Staking operations
- UBI claims

### Wallet Store (`src/store/wallet.ts`)

Global state management using Zustand:
- Wallet initialization and locking
- Account management
- Balance and transaction state
- Network configuration

### Crypto Utilities (`src/utils/crypto.ts`)

Cryptographic functions:
- BIP39 mnemonic generation and validation
- Address derivation and validation
- SHURIUM URI parsing
- Amount formatting

## Network Configuration

### Mainnet
- Default network for real transactions
- P2P Port: 8333
- RPC Port: 8332

### Testnet
- For testing with fake SHR
- Faucet available via `getfaucet` RPC
- P2P Port: 18333
- RPC Port: 18332

### Regtest
- Local development and testing
- Instant block generation
- P2P Port: 18444
- RPC Port: 18443

## Security Considerations

- **Never share your mnemonic phrase**
- Use a strong device passcode
- Enable biometric authentication when available
- Keep your app updated
- Only connect to trusted nodes

## Development

### Running Tests
```bash
npm test
```

### Linting
```bash
npm run lint
```

### Building for Production

iOS:
```bash
cd ios && xcodebuild -workspace ShuriumWallet.xcworkspace -scheme ShuriumWallet -configuration Release
```

Android:
```bash
cd android && ./gradlew assembleRelease
```

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Run tests and linting
5. Submit a pull request

## License

This project is part of the SHURIUM cryptocurrency project.
See the main repository for license information.
