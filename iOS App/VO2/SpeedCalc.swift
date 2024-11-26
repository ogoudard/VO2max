import Foundation
import CoreLocation

class LocationManager: NSObject, ObservableObject, CLLocationManagerDelegate {
    private var locationManager = CLLocationManager()
    var historicalData: [LocationData] = [] // Liste des données historiques
    @Published var speed: Double = 0.0

    override init() {
        super.init()
        locationManager.delegate = self
        locationManager.desiredAccuracy = kCLLocationAccuracyBest // Précision de la localisation

        // Demander l'autorisation d'utiliser la localisation
        if CLLocationManager.locationServicesEnabled() {
            locationManager.requestAlwaysAuthorization()  // Autorisation pour utiliser la localisation en arrière-plan
        } else {
            print("Les services de localisation ne sont pas activés.")
        }
    }

    func startUpdatingLocation() {
        locationManager.startUpdatingLocation()
    }

    func stopUpdatingLocation() {
        locationManager.stopUpdatingLocation()
    }

    // Fonction appelée lors de la mise à jour de la localisation
    func locationManager(_ manager: CLLocationManager, didUpdateLocations locations: [CLLocation]) {
        if let newLocation = locations.last {
            speed = newLocation.speed // Récupère la vitesse en mètres par seconde
            if speed < 0 {
                speed = 0 // Ignore les valeurs de vitesse négatives
            }
            let data = LocationData(time: Date(), speed: speed)
            historicalData.append(data) // Enregistre les données historiques
        }
    }

    // Fonction appelée en cas d'erreur lors de la mise à jour de la localisation
    func locationManager(_ manager: CLLocationManager, didFailWithError error: Error) {
        print("Erreur lors de la mise à jour de la localisation: \(error.localizedDescription)")
    }
}

struct LocationData {
    var time: Date
    var speed: Double
}
