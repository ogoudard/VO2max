import Foundation
import CoreLocation

class LocationManager:NSObject, ObservableObject, CLLocationManagerDelegate{
    private var locationManager = CLLocationManager()
    var historicalData: [LocationData] = [] // Liste des données historiques
    @Published var speed: Double = 0.0
    override init() {
        super.init()
        locationManager.delegate = self
        locationManager.desiredAccuracy = kCLLocationAccuracyNearestTenMeters

        // Demander l'autorisation d'utiliser la localisation
        if CLLocationManager.locationServicesEnabled() {
            // Si l'appareil est prêt à recevoir des mises à jour de localisation, demandez l'autorisation
            locationManager.requestAlwaysAuthorization()  // Utilisez AlwaysAuthorization si vous avez besoin d'accéder à la localisation en arrière-plan
            //locationManager.requestWhenInUseAuthorization() // Sinon, utilisez cette ligne pour l'accès en avant-plan seulement
        } else {
            print("Les services de localisation ne sont pas activés.")
        }
    }

    // Fonction appelée quand une nouvelle mise à jour de localisation est disponible
   // func locationManager(_ manager: CLLocationManager, didUpdateLocations locations: [CLLocation]) {
   //     guard let latestLocation = locations.last else { return }
   //     speed = latestLocation.speed // La vitesse est en mètres par seconde
   //     print("Vitesse actuelle: \(speed) m/s")
   // }

    // Fonction appelée si une erreur se produit lors de la mise à jour de la localisation
    func locationManager(_ manager: CLLocationManager, didFailWithError error: Error) {
        print("Erreur lors de la mise à jour de la localisation: \(error.localizedDescription)")
    }
    func startUpdatingLocation() {
        locationManager.startUpdatingLocation()
    }
    
    func stopUpdatingLocation() {
        locationManager.stopUpdatingLocation()
    }
    
    func locationManager(_ manager: CLLocationManager, didUpdateLocations locations: [CLLocation]) {
        if let newLocation = locations.last {
            speed = newLocation.speed // Récupère la vitesse en m/s
            if speed < 0 {
                speed=0
            }
            let data = LocationData(time: Date(), speed: speed)
              historicalData.append(data)
        }
    }
}
struct LocationData {
    var time: Date
    var speed: Double
}
