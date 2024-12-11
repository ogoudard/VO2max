import SwiftUI
import CoreBluetooth
import Foundation

struct SettingsView: View {
    @ObservedObject var bluetoothManager: BluetoothManager // Ajouter ceci pour observer bluetoothManager
    @ObservedObject var hrbtManager: HRBTManager
    @State private var isVO2HRConnected: Bool? = nil // État de la connexion : nil = inconnu, true = connecté, false = non connecté
    @State private var ishrbtConnected: Bool? = false // État de la connexion : nil = inconnu, true = connecté, false = non connecté
    @State private var buildNumber: String = "Unknown"
    
    var body: some View {
        VStack {
            // Afficher la version et le build
            VStack(alignment: .leading, spacing: 8) {
                Text("App Version: \(AppInfo.version)")
                    .font(.headline)
                Text("Build Number: \(buildNumber)")
                      .onAppear {
                          // Récupérer le numéro de build au moment de l'apparition de la vue
                          if let build = getBuildNumber() {
                              buildNumber = build
                          } else {
                              buildNumber = "Not found"
                          }
                      }
              }
            .font(.subheadline)
            .foregroundColor(.gray)
            .padding()
            
            Spacer()
            
            // Bouton pour rechercher des capteurs de fréquence cardiaque
            Button(action: {
                if !hrbtManager.historicalData.isEmpty {
                    // Les données de la ceinture cardio sont reçues
                    ishrbtConnected = true
                } else {
                    // Aucune donnée reçue de la ceinture cardio
                    ishrbtConnected = false
                }
                 }) {
                Text("Search Heart Rate Sensors")
                    .padding()
                    .background(Color.blue)
                    .foregroundColor(.white)
                    .cornerRadius(8)
            }
            .padding(.bottom, 8) // Espacement en bas du bouton
            
            // Affichage des données de fréquence cardiaque juste sous le bouton
            Text("Heart Rate: \(hrbtManager.heartRate)")
                .font(.headline)
                .padding(.bottom, 16)
                .foregroundColor(getHeartRateColor()) // Appliquer la couleur en fonction de la connexion
 
            // Bouton pour vérifier si VO2-HR est connecté
            Button(action: {
                checkVO2HRConnection()
            }) {
                Text("Check VO2-HR Connection")
                    .padding()
                    .background(Color.blue)
                    .foregroundColor(.white)
                    .cornerRadius(8)
            }
            .padding()
            // Affichage de l'état de connexion de VO2-HR
            if let isConnected = isVO2HRConnected {
                Text(isConnected ? "VO2-HR is Connected" : "VO2-HR is Not Connected")
                    .font(.headline)
                    .foregroundColor(isConnected ? .green : .red)
            } else {
                Text("Connection status: Unknown")
                    .font(.headline)
                    .foregroundColor(.gray)
            }
            
            // Liste des capteurs détectés
            List(hrbtManager.heartRateSensors, id: \.identifier) { sensor in
                Button(action: {
                    hrbtManager.centralManager.connect(sensor, options: nil)
                }) {
                    Text(sensor.name ?? "Inconnu")
                        .padding()
                        .cornerRadius(8)
                }
            }

        }
        .navigationBarTitle("Settings")
        .onAppear {
            hrbtManager.centralManager.scanForPeripherals(withServices: [CBUUID(string: "180D")], options: nil)
        }
    }
    // Fonction pour déterminer la couleur en fonction de la connexion
     private func getHeartRateColor() -> Color {
         if ishrbtConnected == nil {
             return .gray
         }
         if ishrbtConnected == true {
             return .green // Si le capteur est connecté
         } else {
             return .red // Si le capteur n'est pas connecté
         }
     }
    // Méthode pour vérifier la connexion VO2-HR
     private func checkVO2HRConnection() {
         // Utiliser bluetoothManager pour récupérer les périphériques connectés
         let connectedPeripherals = bluetoothManager.centralManager.retrieveConnectedPeripherals(withServices: [CBUUID(string: "b354cf1b-2486-4f21-b4b1-ee4cd5cc3bf0")])

         // Vérifiez si l'un des périphériques a le nom "VO2-HR"
         if connectedPeripherals.contains(where: { $0.name == "VO2-HR" }) {
             isVO2HRConnected = true // Le périphérique VO2-HR est connecté
         } else {
             isVO2HRConnected = false // Le périphérique VO2-HR n'est pas connecté
         }
     }

    // Fonction pour récupérer le CFBundleVersion
    func getBuildNumber() -> String? {
        if let infoDict = Bundle.main.infoDictionary {
            return infoDict["MyBuildVersion"] as? String
        }
        return nil
    }

}

struct SettingsView_Previews: PreviewProvider {
        static var previews: some View {
            SettingsView(bluetoothManager: BluetoothManager(), hrbtManager: HRBTManager())
        }
    }

