import { browser } from '$app/environment';
import en from './en';
import de from './de';
import fr from './fr';

export type Translations = {
	walk: string;
	bike: string;
	car: string;
	from: string;
	to: string;
	arrival: string;
	departure: string;
	duration: string;
	later: string;
	earlier: string;
	arrivals: string;
	departures: string;
	switchToArrivals: string;
	switchToDepartures: string;
	arrivalOnTrack: string;
	track: string;
	tripIntermediateStops: (n: number) => string;
	sharingProvider: string;
};

const translations: Map<string, Translations> = new Map(
	Object.entries({
		en,
		de,
		fr
	})
);

export const language = (browser ? navigator.languages.find((l) => l.length == 2) : 'en') ?? 'en';
export const t = translations.get(language) ?? en;
