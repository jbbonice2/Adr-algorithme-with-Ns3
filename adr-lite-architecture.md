# Schéma d'intégration ADR-Lite dans l'architecture NS-3 LoRaWAN

```
[EndDevice] ──uplink──→ [Gateway(s)] ──P2P──→ [NetworkServer]
                                                    │
                           ┌────────────────────────┘
                           ↓
                    NetworkServer::Receive()
                           │
            ┌──────────────┼───────────────┐
            ↓              ↓               ↓
     NetworkScheduler  NetworkStatus   NetworkController
     (planifie reply)  (stocke paquet      │
                        + rxPower       itère sur
                        par gateway)    m_components
                           │               │
                           └──────→  AdrComponent::BeforeSendingReply()
                                          │
                                    ┌─────┴──────┐
                                    ↓            ↓
                        GetDeviceLiteState()   AdrLiteImplementation()
                        (état binaire par      (recherche binaire sur
                         device)                m_liteConfigurations)
                                    │
                                    ↓
                        ReceivedMatchesAssigned()
                        (succès/échec config)
                                    │
                                    ↓
                        Mise à jour index binaire
                                    │
                                    ↓
                        Nouvelle config (SF, TxP, CR, CF)
                                    │
                                    ↓
                              LinkAdrReq (downlink)
                                    │
                                    ↓
                              [EndDevice]
                        (applique nouvelle config)
```

Ce schéma montre que l’ADR-Lite remplace la logique SNR/RSSI par une recherche binaire sur un espace de configurations, tout en restant intégré au même point d’extension du pipeline.
