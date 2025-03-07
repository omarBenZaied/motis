import type { Translations } from './translation';

const translations: Translations = {
	walk: 'Marche',
	bike: 'Vélo',
	car: 'Voiture',
	from: 'De',
	to: 'À',
	arrival: 'Arrivée',
	departure: 'Départ',
	duration: 'Durée',
	arrivals: 'Arrivées',
	later: 'plus tard',
	earlier: 'plus tôt',
	departures: 'Départ',
	switchToArrivals: 'Basculer vers les arrivées',
	switchToDepartures: 'Basculer vers les départs',
	track: 'Voie',
	arrivalOnTrack: 'Arrivée sur la voie',
	tripIntermediateStops: (n: number) => {
		switch (n) {
			case 0:
				return 'Aucun arrêt intermédiaire';
			case 1:
				return '1 arrêt intermédiaire';
			default:
				return `${n} arrêts intermédiaires`;
		}
	},
	sharingProvider: 'Fournisseur'
};

export default translations;
