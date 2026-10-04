#pragma once

#include "JSONHandler.h"

namespace JSONHandler
{
	class Impact
	{
	public:

		static void ProcessImpactMapping(const json& jsonData)
		{
			int count = 0;

			if (jsonData.contains("ImpactMapping")) {
				impactMappings.reserve(jsonData["ImpactMapping"].size());

				for (const auto& item : jsonData["ImpactMapping"]) {
					ImpactMapping mapping;
					mapping.priority = item.value("Priority", 0);
					mapping.override = item.value("Override", false);
					mapping.overrideMerge = item.value("OverrideMerge", false);

					if (!item.contains("Modifiers") || !item["Modifiers"].is_object()) {
						logger::warn("ImpactMapping [{}] has no Modifiers.\nJSON content : {}", count, item.dump(4));
					}

					ProcessModifiers(item["Modifiers"], mapping);

					ParsedMappingFilters filters;
					if (item.contains("Filters") && item["Filters"].is_object()) {
						auto& itemFilters = item["Filters"];

						if (itemFilters.contains("Victim")) ParseActorFilters(itemFilters["Victim"], filters.victimInc, filters.victimExc, filters.victimAll);
						if (itemFilters.contains("Attacker")) ParseActorFilters(itemFilters["Attacker"], filters.attackerInc, filters.attackerExc, filters.attackerAll);
						if (itemFilters.contains("HitContext")) ParseHitContextFilters(itemFilters["HitContext"], filters.hitInc, filters.hitExc, filters.hitAll);
					}

					mapping.id = count++;
					impactMappings.push_back(std::move(mapping));
					RegisterImpactMapping(impactRegistry, &impactMappings.back(), filters);
				}
			}

			logger::info("ImpactMapping : {} mapping(s) loaded.", count);
		}

		static void ProcessHitMapping(const json& jsonData)
		{
			int count = 0;

			if (jsonData.contains("HitMapping")) {
				hitMappings.reserve(jsonData["HitMapping"].size());

				for (const auto& item : jsonData["HitMapping"]) {
					HitMapping mapping;
					mapping.id = count++;
					mapping.className = item.value("Class", "auto_class_" + std::to_string(mapping.id));
					mapping.priority = item.value("Priority", 0);
					mapping.override = item.value("Override", false);
					mapping.overrideMerge = item.value("OverrideMerge", false);

					if (!item.contains("Modifiers") || !item["Modifiers"].is_object()) {
						logger::warn("HitMapping [{}] has no Modifiers.\nJSON content : {}", count, item.dump(4));
					}

					ProcessModifiers(item["Modifiers"], mapping);
					const auto deferred = item.value("Deferred", false);
					mapping.modifier.deferred = deferred;

					ParsedMappingFilters filters;
					if (item.contains("Filters") && item["Filters"].is_object()) {
						auto& itemFilters = item["Filters"];

						if (itemFilters.contains("Victim")) ParseActorFilters(itemFilters["Victim"], filters.victimInc, filters.victimExc, filters.victimAll);
						if (itemFilters.contains("Attacker")) ParseActorFilters(itemFilters["Attacker"], filters.attackerInc, filters.attackerExc, filters.attackerAll);
						if (itemFilters.contains("HitContext")) ParseHitContextFilters(itemFilters["HitContext"], filters.hitInc, filters.hitExc, filters.hitAll);
					}

					hitMappings.push_back(std::move(mapping));
					RegisterImpactMapping(hitRegistry, &hitMappings.back(), filters);
				}
			}

			logger::info("HitMapping : {} mapping(s) loaded.", count);
		}

	private:

		template <typename T>
		static void ProcessModifiers(const json& modifiers, T& mapping)
		{
			using namespace ModData;

			auto& mod = mapping.modifier;

			if constexpr (std::is_same_v<T, ImpactMapping>) {
				if (modifiers.contains("ImpactData") && modifiers["ImpactData"].is_string()) {
					mod.impactData = MiscUtils::GetFormFromAssoc<RE::BGSImpactData>(modifiers.value("ImpactData", ""));
				} else mod.impactData = nullptr;

				if (modifiers.contains("SoundOverride") && modifiers["SoundOverride"].is_string()) {
					mod.soundOverride = MiscUtils::GetFormFromAssoc<RE::BGSSoundDescriptorForm>(modifiers.value("SoundOverride", ""));
				} else mod.soundOverride = nullptr;

				if (modifiers.contains("DecalOverride") && modifiers["DecalOverride"].is_string()) {
					mod.decalOverride = MiscUtils::GetFormFromAssoc<RE::BGSImpactData>(modifiers.value("DecalOverride", ""));
				} else mod.decalOverride = nullptr;

				if (modifiers.contains("PreserveOriginalSound") && modifiers["PreserveOriginalSound"].is_boolean()) {
					mod.preserveOriginalSound = modifiers["PreserveOriginalSound"].get<bool>();
				} else mod.preserveOriginalSound = std::nullopt;

				if (modifiers.contains("PreserveOriginalDecal") && modifiers["PreserveOriginalDecal"].is_boolean()) {
					mod.preserveOriginalDecal = modifiers["PreserveOriginalDecal"].get<bool>();
				} else mod.preserveOriginalDecal = std::nullopt;

				if (modifiers.contains("RemoveDecal") && modifiers["RemoveDecal"].is_boolean()) {
					mod.removeDecal = modifiers["RemoveDecal"].get<bool>();
				} else mod.removeDecal = std::nullopt;

				if (modifiers.contains("RemoveBloodSplatter") && modifiers["RemoveBloodSplatter"].is_boolean()) {
					mod.removeBloodSplatter = modifiers["RemoveBloodSplatter"].get<bool>();
				} else mod.removeBloodSplatter = std::nullopt;

				if (modifiers.contains("RemoveSound") && modifiers["RemoveSound"].is_boolean()) {
					mod.removeSound = modifiers["RemoveSound"].get<bool>();
				} else mod.removeSound = std::nullopt;

				if (modifiers.contains("ImpactBounce") && modifiers["ImpactBounce"].is_boolean()) {
					mod.impactBounce = modifiers["ImpactBounce"].get<bool>();
				} else mod.impactBounce = std::nullopt;
			}

			if (modifiers.contains("BloodSpray") && modifiers["BloodSpray"].is_string()) {
				mod.bloodSpray = MiscUtils::GetFormFromAssoc<RE::SpellItem>(modifiers.value("BloodSpray", ""));
				if (mod.bloodSpray && !mod.bloodSpray->effects.empty()) {
					if (RE::EffectSetting* firstEffect = mod.bloodSpray->effects[0]->baseEffect) {
						if (firstEffect->data.projectileBase) {
							firstEffect->data.projectileBase->data.collisionLayer = ModRuntimeBlood_CollisionLayer;
							if (spellProjectileMap.find(mod.bloodSpray) == spellProjectileMap.end()) {
								spellProjectileMap[mod.bloodSpray] = firstEffect->data.projectileBase;
							}
						}
					}
				}
			} else mod.bloodSpray = nullptr;

			if (modifiers.contains("Stagger") && !modifiers["Stagger"].is_null()) {
				const auto& val = modifiers["Stagger"];
				if (val.is_number()) {
					mod.stagger = val.get<float>();
				} else if (val.is_string()) {
					auto* global = Main::ResolveForm<RE::TESGlobal>(val.get<std::string>(), true);
					if (global) mod.stagger = global;
				}
			}

			if (modifiers.contains("Disarm") && modifiers["Disarm"].is_boolean()) {
				mod.disarm = modifiers["Disarm"].get<bool>();
			} else mod.disarm = std::nullopt;

			if (modifiers.contains("Eject") && modifiers["Eject"].is_boolean()) {
				mod.eject = modifiers["Eject"].get<bool>();
			} else mod.eject = std::nullopt;

			if (modifiers.contains("DamageMult")) {
				const auto& value = modifiers["DamageMult"];
				if (value.is_number()) {
					mod.damageMult = value.get<float>();
				} else if (value.is_string()) {
					auto* global = Main::ResolveForm<RE::TESGlobal>(value, true);
					if (global) mod.damageMult = global;
				}
			}

			if (modifiers.contains("DamageLimbMult") && !modifiers["DamageLimbMult"].is_null()) {
				const auto& val = modifiers["DamageLimbMult"];
				if (val.is_number()) {
					mod.damageLimbMult = val.get<float>();
				} else if (val.is_string()) {
					auto* global = Main::ResolveForm<RE::TESGlobal>(val.get<std::string>(), true);
					if (global) mod.damageLimbMult = global;
				}
			}

			if (modifiers.contains("Dismember") && modifiers["Dismember"].is_boolean()) {
				mod.dismemberAuto = modifiers["Dismember"].get<bool>();
			} else mod.dismemberAuto = std::nullopt;

			if (modifiers.contains("Bloodpool") && !modifiers["Bloodpool"].is_null()) {
				auto entries = modifiers["Bloodpool"].is_array() ? modifiers["Bloodpool"] : json::array({ modifiers["Bloodpool"] });
				for (const auto& entry : entries) {
					if (!entry.is_string()) continue;
					mod.bloodpools.push_back(entry);
				}
			}

			if (modifiers.contains("ExtraImpactData") && !modifiers["ExtraImpactData"].is_null()) {
				auto entries = modifiers["ExtraImpactData"].is_array() ? modifiers["ExtraImpactData"] : json::array({ modifiers["ExtraImpactData"] });
				for (const auto& e : entries) {
					if (!e.is_string()) continue;
					const std::string s = e.get<std::string>();
					auto lower = s | std::views::transform(::tolower) | std::ranges::to<std::string>();
					if (lower.ends_with(".nif")) mod.extraImpactData.push_back(s);
					else if (auto* form = MiscUtils::GetFormFromAssoc<RE::BGSImpactData>(s)) mod.extraImpactData.push_back(form);
				}
			}

			Main::ParseMappingFilterFormJson<RE::BGSSoundDescriptorForm, RE::BGSSoundDescriptorForm*>(modifiers, "ExtraSound", mod.extraSound);
			Main::ParseMappingFilterFormJson<RE::SpellItem, RE::SpellItem*>(modifiers, "Spells", mod.spells);
			Main::ParseMappingFilterFormJson<RE::TESForm, RE::TESForm*>(modifiers, "PlacedObjects", mod.placedObjects);
		}

		static void ParseActorFilters(const json& j, ActorFilters& inc, ActorFilters& exc, ActorFilters& all)
		{
			using Filter = Filter;

			auto parseSex = [&](const std::string& key, std::vector<Filter::ActorSex>& out) {
				if (!j.contains(key) || j[key].is_null()) return;
				auto entries = j[key].is_array() ? j[key] : json::array({ j[key] });
				for (const auto& e : entries) {
					if (!e.is_string()) continue;
					std::string s = e.get<std::string>();
					if (s == "Male") out.push_back(Filter::ActorSex::kMale);
					else if (s == "Female") out.push_back(Filter::ActorSex::kFemale);
				}
			};

			auto parseSkeletons = [&](const std::string& key, std::vector<RE::FormID>& out) {
				if (!j.contains(key) || j[key].is_null()) return;
				auto entries = j[key].is_array() ? j[key] : json::array({ j[key] });
				for (const auto& e : entries) {
					if (!e.is_string()) continue;
					std::string s = e.get<std::string>();
					std::ranges::transform(s, s.begin(), [](unsigned char c) { return std::tolower(c); });
					out.push_back(static_cast<RE::FormID>(std::hash<std::string>{}(s)));
				}
			};

			Main::ParseMappingFilterFormJson<RE::TESRace, RE::FormID>(j, "Races", inc.races, true);
			Main::ParseMappingFilterFormJson<RE::TESRace, RE::FormID>(j, "!Races", exc.races, true);
			Main::ParseMappingFilterFormJson<RE::BGSKeyword, RE::FormID>(j, "Keywords", inc.keywords, true);
			Main::ParseMappingFilterFormJson<RE::BGSKeyword, RE::FormID>(j, "!Keywords", exc.keywords, true);
			Main::ParseMappingFilterFormJson<RE::BGSKeyword, RE::FormID>(j, "&Keywords", all.keywords, true);
			Main::ParseMappingFilterFormJson<RE::TESObjectARMO, RE::FormID>(j, "Skins", inc.skins, false);
			Main::ParseMappingFilterFormJson<RE::TESObjectARMO, RE::FormID>(j, "!Skins", exc.skins, false);
			Main::ParseMappingFilterFormJson<RE::BGSMaterialType, RE::FormID>(j, "Materials", inc.materials, false);
			Main::ParseMappingFilterFormJson<RE::BGSMaterialType, RE::FormID>(j, "!Materials", exc.materials, false);
			Main::ParseMappingFilterFormJson<RE::TESBoundObject, RE::FormID>(j, "Worn", inc.worn, false);
			Main::ParseMappingFilterFormJson<RE::TESBoundObject, RE::FormID>(j, "!Worn", exc.worn, false);
			Main::ParseMappingFilterFormJson<RE::TESBoundObject, RE::FormID>(j, "&Worn", all.worn, false);
			Main::ParseMappingFilterFormJson<RE::BGSKeyword, RE::FormID>(j, "WornKeywords", inc.wornKeywords, true);
			Main::ParseMappingFilterFormJson<RE::BGSKeyword, RE::FormID>(j, "!WornKeywords", exc.wornKeywords, true);
			Main::ParseMappingFilterFormJson<RE::BGSKeyword, RE::FormID>(j, "&WornKeywords", all.wornKeywords, true);
			Main::ParseMappingFilterFormJson<RE::BGSPerk, RE::FormID>(j, "Perks", inc.perks, false);
			Main::ParseMappingFilterFormJson<RE::BGSPerk, RE::FormID>(j, "!Perks", exc.perks, false);
			Main::ParseMappingFilterFormJson<RE::BGSPerk, RE::FormID>(j, "&Perks", all.perks, false);
			Main::ParseMappingFilterFormJson<RE::SpellItem, RE::FormID>(j, "Spells", inc.spells, false);
			Main::ParseMappingFilterFormJson<RE::SpellItem, RE::FormID>(j, "!Spells", exc.spells, false);
			Main::ParseMappingFilterFormJson<RE::SpellItem, RE::FormID>(j, "&Spells", all.spells, false);
			Main::ParseMappingFilterFormJson<RE::EffectSetting, RE::FormID>(j, "MagicEffects", inc.magicEffects, false);
			Main::ParseMappingFilterFormJson<RE::EffectSetting, RE::FormID>(j, "!MagicEffects", exc.magicEffects, false);
			Main::ParseMappingFilterFormJson<RE::EffectSetting, RE::FormID>(j, "&MagicEffects", all.magicEffects, false);
			Main::ParseMappingFilterFormJson<RE::TESForm, RE::FormID>(j, "FormID", inc.formIds, false);
			Main::ParseMappingFilterFormJson<RE::TESForm, RE::FormID>(j, "!FormID", exc.formIds, false);

			parseSkeletons("Skeletons", inc.skeletons);
			parseSkeletons("!Skeletons", exc.skeletons);
			parseSex("Sex", inc.sexes);
			parseSex("!Sex", exc.sexes);
		}

		static void ParseHitContextFilters(const json& j, HitContextFilters& inc, HitContextFilters& exc, HitContextFilters& all)
		{
			using Filter = Filter;

			auto parseVariantFloat = [&](const std::string& key, std::variant<float, RE::TESGlobal*>& out) {
				if (!j.contains(key) || j[key].is_null()) return;
				const auto& val = j[key];
				if (val.is_number()) {
					out = val.get<float>();
				} else if (val.is_string()) {
					auto* global = Main::ResolveForm<RE::TESGlobal>(val.get<std::string>(), true);
					if (global) out = global;
				}
			};

			auto parseEnum = [&](const std::string& key, auto& out, auto parser) {
				if (!j.contains(key) || j[key].is_null()) return;
				auto entries = j[key].is_array() ? j[key] : json::array({ j[key] });
				for (const auto& e : entries) {
					if (!e.is_string()) continue;
					parser(e.get<std::string>(), out);
				}
			};

			auto parseWeaponType = [](const std::string& s, std::vector<Filter::WeaponType>& out) {
				if (s == "OneHandSword") out.push_back(Filter::WeaponType::kOneHandSword);
				else if (s == "TwoHandSword") out.push_back(Filter::WeaponType::kTwoHandSword);
				else if (s == "OneHandAxe") out.push_back(Filter::WeaponType::kOneHandAxe);
				else if (s == "TwoHandAxe") out.push_back(Filter::WeaponType::kTwoHandAxe);
				else if (s == "OneHandMace") out.push_back(Filter::WeaponType::kOneHandMace);
				else if (s == "TwoHandMace") out.push_back(Filter::WeaponType::kTwoHandMace);
				else if (s == "Dagger") out.push_back(Filter::WeaponType::kDagger);
				else if (s == "Ranged") out.push_back(Filter::WeaponType::kRanged);
				else if (s == "Magic") out.push_back(Filter::WeaponType::kMagic);
				else if (s == "HandToHand") out.push_back(Filter::WeaponType::kHandToHand);
				else if (s == "Beast") out.push_back(Filter::WeaponType::kBeast);
				else if (s == "Other") out.push_back(Filter::WeaponType::kOther);
			};

			auto parseArmorClass = [](const std::string& s, std::vector<Filter::ArmorClassType>& out) {
				if (s == "Heavy") out.push_back(Filter::ArmorClassType::kHeavy);
				else if (s == "Light") out.push_back(Filter::ArmorClassType::kLight);
				else if (s == "Cloth") out.push_back(Filter::ArmorClassType::kCloth);
				else if (s == "Default") out.push_back(Filter::ArmorClassType::kDefault);
			};

			auto parseBlocked = [](const std::string& s, std::vector<Filter::BlockedFilter>& out) {
				if (s == "ShieldHeavy") out.push_back(Filter::BlockedFilter::kShieldHeavy);
				else if (s == "ShieldLight") out.push_back(Filter::BlockedFilter::kShieldLight);
				else if (s == "Weapon") out.push_back(Filter::BlockedFilter::kWeapon);
				else if (s == "No") out.push_back(Filter::BlockedFilter::kNo);
			};

			auto parseState = [](const std::string& s, std::vector<Filter::StateFilter>& out) {
				if (s == "Alive") out.push_back(Filter::StateFilter::kAlive);
				else if (s == "Dying") out.push_back(Filter::StateFilter::kDying);
				else if (s == "Killmove") out.push_back(Filter::StateFilter::kKillmove);
				else if (s == "Dead") out.push_back(Filter::StateFilter::kDead);
			};

			auto parseAttack = [](const std::string& s, std::vector<Filter::AttackFilter>& out) {
				if (s == "Regular") out.push_back(Filter::AttackFilter::kRegular);
				else if (s == "Power") out.push_back(Filter::AttackFilter::kPower);
				else if (s == "Bash") out.push_back(Filter::AttackFilter::kBash);
			};

			auto parseSource = [](const std::string& s, std::vector<Filter::SourceFilter>& out) {
				if (s == "RightHand") out.push_back(Filter::SourceFilter::kRightHand);
				else if (s == "LeftHand") out.push_back(Filter::SourceFilter::kLeftHand);
				else if (s == "DualHand") out.push_back(Filter::SourceFilter::kDualHand);
				else if (s == "Shout") out.push_back(Filter::SourceFilter::kShout);
				else if (s == "Other") out.push_back(Filter::SourceFilter::kOther);
			};

			auto parseCritical = [](const std::string& s, std::vector<Filter::CriticalAttackFilter>& out) {
				if (s == "Yes") out.push_back(Filter::CriticalAttackFilter::kYes);
				else if (s == "No") out.push_back(Filter::CriticalAttackFilter::kNo);
			};

			auto parseSneak = [](const std::string& s, std::vector<Filter::SneakAttackFilter>& out) {
				if (s == "Yes") out.push_back(Filter::SneakAttackFilter::kYes);
				else if (s == "No") out.push_back(Filter::SneakAttackFilter::kNo);
			};

			auto parseGlobales = [&](const std::string& key, std::vector<Filter::GlobalFilter>& out) {
				if (!j.contains(key) || j[key].is_null()) return;
				auto entries = j[key].is_array() ? j[key] : json::array({ j[key] });
				for (const auto& g : entries) {
					if (!g.is_array() || g.size() != 3) continue;
					auto* global = Main::ResolveForm<RE::TESGlobal>(g[0].get<std::string>(), true);
					if (!global) continue;
					auto cmp = Main::ToComparisonType(g[1].get<std::string>());
					if (cmp == Filter::ComparisonType::kInvalid) continue;
					out.emplace_back(global, cmp, g[2].get<float>());
				}
			};

			auto parseBipedSlots = [&](const std::string& key, std::vector<int>& out) {
				if (!j.contains(key) || j[key].is_null()) return;
				auto entries = j[key].is_array() ? j[key] : json::array({ j[key] });
				for (const auto& e : entries) {
					if (e.is_number_integer()) out.push_back(e.get<int>());
				}
			};

			auto parseBipedStrings = [&](const std::string& key, std::vector<std::string>& out) {
				if (!j.contains(key) || j[key].is_null()) return;
				auto entries = j[key].is_array() ? j[key] : json::array({ j[key] });
				for (const auto& e : entries) {
					if (e.is_string()) out.push_back(e.get<std::string>());
				}
			};

			Main::ParseMappingFilterFormJson<RE::TESObjectWEAP, RE::FormID>(j, "Weapons", inc.weapons, false);
			Main::ParseMappingFilterFormJson<RE::TESObjectWEAP, RE::FormID>(j, "!Weapons", exc.weapons, false);
			Main::ParseMappingFilterFormJson<RE::BGSKeyword, RE::FormID>(j, "WeaponKeywords", inc.weaponKeywords, true);
			Main::ParseMappingFilterFormJson<RE::BGSKeyword, RE::FormID>(j, "!WeaponKeywords", exc.weaponKeywords, true);
			Main::ParseMappingFilterFormJson<RE::BGSKeyword, RE::FormID>(j, "&WeaponKeywords", all.weaponKeywords, true);
			Main::ParseMappingFilterFormJson<RE::MagicItem, RE::FormID>(j, "MagicItems", inc.magicItems, false);
			Main::ParseMappingFilterFormJson<RE::MagicItem, RE::FormID>(j, "!MagicItems", exc.magicItems, false);
			Main::ParseMappingFilterFormJson<RE::BGSProjectile, RE::FormID>(j, "Projectiles", inc.projectiles, false);
			Main::ParseMappingFilterFormJson<RE::BGSProjectile, RE::FormID>(j, "!Projectiles", exc.projectiles, false);
			Main::ParseMappingFilterFormJson<RE::EffectSetting, RE::FormID>(j, "MagicEffects", inc.magicEffects, false);
			Main::ParseMappingFilterFormJson<RE::EffectSetting, RE::FormID>(j, "!MagicEffects", exc.magicEffects, false);
			Main::ParseMappingFilterFormJson<RE::TESObjectARMO, RE::FormID>(j, "Armors", inc.armors, false);
			Main::ParseMappingFilterFormJson<RE::TESObjectARMO, RE::FormID>(j, "!Armors", exc.armors, false);
			Main::ParseMappingFilterFormJson<RE::BGSKeyword, RE::FormID>(j, "ArmorKeywords", inc.armorKeywords, true);
			Main::ParseMappingFilterFormJson<RE::BGSKeyword, RE::FormID>(j, "!ArmorKeywords", exc.armorKeywords, true);
			Main::ParseMappingFilterFormJson<RE::BGSKeyword, RE::FormID>(j, "&ArmorKeywords", all.armorKeywords, true);
			Main::ParseMappingFilterFormJson<RE::BGSPerk, RE::BGSPerk*>(j, "Conditions", inc.conditionsAny, false);
			Main::ParseMappingFilterFormJson<RE::BGSPerk, RE::BGSPerk*>(j, "!Conditions", exc.conditionsNone, false);
			Main::ParseMappingFilterFormJson<RE::BGSPerk, RE::BGSPerk*>(j, "&Conditions", all.conditionsAll, false);

			parseEnum("WeaponTypes", inc.weaponTypes, parseWeaponType);
			parseEnum("!WeaponTypes", exc.weaponTypes, parseWeaponType);
			parseEnum("ArmorClasses", inc.armorClasses, parseArmorClass);
			parseEnum("!ArmorClasses", exc.armorClasses, parseArmorClass);
			parseEnum("Blocked", inc.blockeds, parseBlocked);
			parseEnum("!Blocked", exc.blockeds, parseBlocked);
			parseEnum("States", inc.states, parseState);
			parseEnum("!States", exc.states, parseState);
			parseEnum("Attacks", inc.attacks, parseAttack);
			parseEnum("!Attacks", exc.attacks, parseAttack);
			parseEnum("Sources", inc.sources, parseSource);
			parseEnum("!Sources", exc.sources, parseSource);
			parseEnum("Critical", inc.criticals, parseCritical);
			parseEnum("!Critical", exc.criticals, parseCritical);
			parseEnum("Sneak", inc.sneaks, parseSneak);
			parseEnum("!Sneak", exc.sneaks, parseSneak);

			parseGlobales("Globales", inc.globalesAny);
			parseGlobales("!Globales", exc.globalesNone);
			parseGlobales("&Globales", all.globalesAll);
			parseBipedSlots("BipedSlots", inc.bipedSlots);
			parseBipedSlots("!BipedSlots", exc.bipedSlots);
			parseBipedSlots("&BipedSlots", all.bipedSlots);
			parseBipedStrings("BipedLimbs", inc.bipedLimbs);
			parseBipedStrings("!BipedLimbs", exc.bipedLimbs);
			parseBipedStrings("BipedNodes", inc.bipedNodes);
			parseBipedStrings("!BipedNodes", exc.bipedNodes);
			parseBipedStrings("BipedKeys", inc.bipedKeys);
			parseBipedStrings("!BipedKeys", exc.bipedKeys);
			parseBipedStrings("&BipedKeys", all.bipedKeys);

			parseVariantFloat("Percentage", inc.percentage);
			parseVariantFloat("PercentageMult", inc.percentageMult);
			parseVariantFloat("MaxHealth", inc.maxHealth);
			parseVariantFloat("MinDamage", inc.minDamage);
			parseVariantFloat("MaxLimbHealth", inc.maxLimbHealth);
		}

		template <typename NodeType, typename MappingType>
		static void FillActorTrie(NodeType& root, const ActorFilters& f, MappingType* mapping, TrieFillMode mode)
		{
			switch (mode) {
				case TrieFillMode::kExclude: {
					std::vector<std::pair<uint8_t, RE::FormID>> noneFilters;

					for (auto id : f.races) noneFilters.emplace_back((uint8_t)TrieLevelActor::kRace, id);
					for (auto id : f.keywords) noneFilters.emplace_back((uint8_t)TrieLevelActor::kKeyword, id);
					for (auto id : f.skins) noneFilters.emplace_back((uint8_t)TrieLevelActor::kSkin, id);
					for (auto sx : f.sexes) noneFilters.emplace_back((uint8_t)TrieLevelActor::kSex, (RE::FormID)sx);
					for (auto id : f.materials) noneFilters.emplace_back((uint8_t)TrieLevelActor::kMaterial, id);
					for (auto id : f.worn) noneFilters.emplace_back((uint8_t)TrieLevelActor::kWorn, id);
					for (auto id : f.wornKeywords) noneFilters.emplace_back((uint8_t)TrieLevelActor::kWornKeyword, id);
					for (auto id : f.perks) noneFilters.emplace_back((uint8_t)TrieLevelActor::kPerk, id);
					for (auto id : f.spells) noneFilters.emplace_back((uint8_t)TrieLevelActor::kSpell, id);
					for (auto id : f.magicEffects) noneFilters.emplace_back((uint8_t)TrieLevelActor::kMagicEffect, id);
					for (auto id : f.skeletons) noneFilters.emplace_back((uint8_t)TrieLevelActor::kSkeleton, id);
					for (auto id : f.formIds) noneFilters.emplace_back((uint8_t)TrieLevelActor::kFormID, id);

					if (noneFilters.empty()) return;

					auto it = std::ranges::find_if(root.buckets, [&](const auto& b) {
						return b.noneFilters == noneFilters;
					});

					if (it == root.buckets.end()) {
						root.buckets.push_back({
							.idMapping = mapping->id,
							.noneFilters = noneFilters });
						it = root.buckets.end() - 1;
					}

					it->mappings.push_back(mapping);
					return;
				}
				case TrieFillMode::kAll: {
					std::vector<std::pair<uint8_t, RE::FormID>> allFilters;
					for (auto id : f.races) allFilters.emplace_back((uint8_t)TrieLevelActor::kRace, id);
					for (auto id : f.keywords) allFilters.emplace_back((uint8_t)TrieLevelActor::kKeyword, id);
					for (auto id : f.skins) allFilters.emplace_back((uint8_t)TrieLevelActor::kSkin, id);
					for (auto id : f.materials) allFilters.emplace_back((uint8_t)TrieLevelActor::kMaterial, id);
					for (auto id : f.worn) allFilters.emplace_back((uint8_t)TrieLevelActor::kWorn, id);
					for (auto id : f.wornKeywords) allFilters.emplace_back((uint8_t)TrieLevelActor::kWornKeyword, id);
					for (auto id : f.perks) allFilters.emplace_back((uint8_t)TrieLevelActor::kPerk, id);
					for (auto id : f.spells) allFilters.emplace_back((uint8_t)TrieLevelActor::kSpell, id);
					for (auto id : f.magicEffects) allFilters.emplace_back((uint8_t)TrieLevelActor::kMagicEffect, id);
					for (auto id : f.formIds) allFilters.emplace_back((uint8_t)TrieLevelActor::kFormID, id);
					if (allFilters.empty()) return;

					auto it = std::ranges::find_if(root.buckets, [&](const auto& b) {
						return b.allFilters == allFilters;
					});
					if (it == root.buckets.end()) {
						root.buckets.push_back({ .idMapping = mapping->id, .allFilters = allFilters });
						it = root.buckets.end() - 1;
					}
					it->mappings.push_back(mapping);
					return;
				}
				case TrieFillMode::kInclude:
				default: break;
			}

			auto toIDs = [](const std::vector<RE::FormID>& v) { return v.empty() ? std::vector<RE::FormID>{0x0} : v; };
			auto toEnum = [](const auto& v, auto any) {
				using T = std::decay_t<decltype(any)>;
				return v.empty() ? std::vector<T>{any} : v;
			};

			for (auto race : toIDs(f.races))
			for (auto keyword : toIDs(f.keywords))
			for (auto skin : toIDs(f.skins))
			for (auto sex : toEnum(f.sexes, Filter::ActorSex::kAny))
			for (auto material : toIDs(f.materials))
			for (auto worn : toIDs(f.worn))
			for (auto wornKeyword : toIDs(f.wornKeywords))
			for (auto perk : toIDs(f.perks))
			for (auto spell : toIDs(f.spells))
			for (auto magicEffect : toIDs(f.magicEffects))
			for (auto skeleton : toIDs(f.skeletons))
			for (auto formID : toIDs(f.formIds)) {
				NodeType* node = &root;
				node = &node->children[TrieKey((uint8_t)TrieLevelActor::kRace, race)];
				node = &node->children[TrieKey((uint8_t)TrieLevelActor::kKeyword, keyword)];
				node = &node->children[TrieKey((uint8_t)TrieLevelActor::kSkin, skin)];
				node = &node->children[TrieKey((uint8_t)TrieLevelActor::kSex, (uint32_t)sex)];
				node = &node->children[TrieKey((uint8_t)TrieLevelActor::kMaterial, material)];
				node = &node->children[TrieKey((uint8_t)TrieLevelActor::kWorn, worn)];
				node = &node->children[TrieKey((uint8_t)TrieLevelActor::kWornKeyword, wornKeyword)];
				node = &node->children[TrieKey((uint8_t)TrieLevelActor::kPerk, perk)];
				node = &node->children[TrieKey((uint8_t)TrieLevelActor::kSpell, spell)];
				node = &node->children[TrieKey((uint8_t)TrieLevelActor::kMagicEffect, magicEffect)];
				node = &node->children[TrieKey((uint8_t)TrieLevelActor::kSkeleton, skeleton)];
				node = &node->children[TrieKey((uint8_t)TrieLevelActor::kFormID, formID)];

				if (node->buckets.empty()) {
					node->buckets.push_back({ .idMapping = mapping->id });
				}
				node->buckets.back().mappings.push_back(mapping);
			}
		}

		template <typename NodeType, typename MappingType>
		static void FillHitContextTrie(NodeType& root, const HitContextFilters& f, MappingType* mapping, TrieFillMode mode)
		{
			static constexpr uint32_t kAnySlot = 0x0;
			static constexpr uint32_t kAnyStrHash = 0x0;

			auto hashStr = [](const std::string& s) -> uint32_t {
				return static_cast<uint32_t>(std::hash<std::string>{}(s));
			};

			auto toSlots = [](const std::vector<int>& v) -> std::vector<uint32_t> {
				if (v.empty()) return { kAnySlot };
				std::vector<uint32_t> out; out.reserve(v.size());
				for (auto s : v) out.push_back(static_cast<uint32_t>(s));
				return out;
			};

			auto toStrHashes = [&](const std::vector<std::string>& v) -> std::vector<uint32_t> {
				if (v.empty()) return { kAnyStrHash };
				std::vector<uint32_t> out; out.reserve(v.size());
				for (const auto& s : v) out.push_back(hashStr(s));
				return out;
			};

			switch (mode) {
				case TrieFillMode::kExclude: {
					std::vector<std::pair<uint8_t, RE::FormID>> noneFilters;

					for (auto id : f.weapons) noneFilters.emplace_back((uint8_t)TrieLevelHitContext::kWeapon, id);
					for (auto id : f.weaponKeywords) noneFilters.emplace_back((uint8_t)TrieLevelHitContext::kWeaponKeyword, id);
					for (auto id : f.magicItems) noneFilters.emplace_back((uint8_t)TrieLevelHitContext::kMagicItem, id);
					for (auto id : f.projectiles) noneFilters.emplace_back((uint8_t)TrieLevelHitContext::kProjectile, id);
					for (auto id : f.magicEffects) noneFilters.emplace_back((uint8_t)TrieLevelHitContext::kMagicEffect, id);
					for (auto id : f.armors) noneFilters.emplace_back((uint8_t)TrieLevelHitContext::kArmor, id);
					for (auto id : f.armorKeywords) noneFilters.emplace_back((uint8_t)TrieLevelHitContext::kArmorKeyword, id);
					for (auto wt : f.weaponTypes) noneFilters.emplace_back((uint8_t)TrieLevelHitContext::kWeaponType, static_cast<RE::FormID>(wt));
					for (auto ac : f.armorClasses) noneFilters.emplace_back((uint8_t)TrieLevelHitContext::kArmorClass, static_cast<RE::FormID>(ac));
					for (auto bl : f.blockeds) noneFilters.emplace_back((uint8_t)TrieLevelHitContext::kBlocked, static_cast<RE::FormID>(bl));
					for (auto st : f.states) noneFilters.emplace_back((uint8_t)TrieLevelHitContext::kState, static_cast<RE::FormID>(st));
					for (auto atk : f.attacks) noneFilters.emplace_back((uint8_t)TrieLevelHitContext::kAttack, static_cast<RE::FormID>(atk));
					for (auto src : f.sources) noneFilters.emplace_back((uint8_t)TrieLevelHitContext::kSource, static_cast<RE::FormID>(src));
					for (auto crit : f.criticals) noneFilters.emplace_back((uint8_t)TrieLevelHitContext::kCritical, static_cast<RE::FormID>(crit));
					for (auto snk : f.sneaks) noneFilters.emplace_back((uint8_t)TrieLevelHitContext::kSneak, static_cast<RE::FormID>(snk));
					for (auto slot : f.bipedSlots) noneFilters.emplace_back((uint8_t)TrieLevelHitContext::kBipedSlot, static_cast<RE::FormID>(slot));
					for (const auto& s : f.bipedLimbs) noneFilters.emplace_back((uint8_t)TrieLevelHitContext::kBipedLimb, hashStr(s));
					for (const auto& s : f.bipedNodes) noneFilters.emplace_back((uint8_t)TrieLevelHitContext::kBipedNode, hashStr(s));
					for (const auto& s : f.bipedKeys) noneFilters.emplace_back((uint8_t)TrieLevelHitContext::kBipedKey, hashStr(s));

					if (noneFilters.empty() && f.conditionsNone.empty() && f.globalesNone.empty()) return;

					auto it = std::ranges::find_if(root.buckets, [&](const auto& b) {
						return b.noneFilters == noneFilters &&
							 b.conditionsNone == f.conditionsNone &&
							 b.globalesNone == f.globalesNone;
					});

					if (it == root.buckets.end()) {
						root.buckets.push_back({
							.idMapping = mapping->id,
							.globalesNone = f.globalesNone,
							.conditionsNone = f.conditionsNone,
							.noneFilters = noneFilters });
						it = root.buckets.end() - 1;
					}

					it->mappings.push_back(mapping);
					return;
				}

				case TrieFillMode::kAll: {
					std::vector<std::pair<uint8_t, RE::FormID>> allFilters;
					for (auto id : f.weapons) allFilters.emplace_back((uint8_t)TrieLevelHitContext::kWeapon, id);
					for (auto id : f.weaponKeywords) allFilters.emplace_back((uint8_t)TrieLevelHitContext::kWeaponKeyword,id);
					for (auto id : f.magicItems) allFilters.emplace_back((uint8_t)TrieLevelHitContext::kMagicItem, id);
					for (auto id : f.projectiles) allFilters.emplace_back((uint8_t)TrieLevelHitContext::kProjectile, id);
					for (auto id : f.magicEffects) allFilters.emplace_back((uint8_t)TrieLevelHitContext::kMagicEffect, id);
					for (auto id : f.armors) allFilters.emplace_back((uint8_t)TrieLevelHitContext::kArmor, id);
					for (auto id : f.armorKeywords) allFilters.emplace_back((uint8_t)TrieLevelHitContext::kArmorKeyword, id);
					for (auto slot : f.bipedSlots) allFilters.emplace_back((uint8_t)TrieLevelHitContext::kBipedSlot, static_cast<uint32_t>(slot));
					for (const auto& s : f.bipedLimbs) allFilters.emplace_back((uint8_t)TrieLevelHitContext::kBipedLimb, static_cast<uint32_t>(hashStr(s)));
					for (const auto& s : f.bipedNodes) allFilters.emplace_back((uint8_t)TrieLevelHitContext::kBipedNode, static_cast<uint32_t>(hashStr(s)));
					for (const auto& s : f.bipedKeys) allFilters.emplace_back((uint8_t)TrieLevelHitContext::kBipedKey, static_cast<uint32_t>(hashStr(s)));
					if (allFilters.empty() &&
						f.conditionsAny.empty() && f.conditionsAll.empty() && f.conditionsNone.empty() &&
						f.globalesAny.empty() && f.globalesAll.empty() && f.globalesNone.empty()) return;

					auto it = std::ranges::find_if(root.buckets, [&](const auto& b) {
						return b.allFilters == allFilters &&
							 b.conditionsAny == f.conditionsAny && b.conditionsAll == f.conditionsAll && b.conditionsNone == f.conditionsNone &&
							 b.globalesAny == f.globalesAny && b.globalesAll == f.globalesAll && b.globalesNone == f.globalesNone;
					});
					if (it == root.buckets.end()) {
						root.buckets.push_back({ .idMapping = mapping->id,
							.globalesAny = f.globalesAny,
							.globalesAll = f.globalesAll,
							.globalesNone = f.globalesNone,
							.conditionsAny = f.conditionsAny,
							.conditionsAll = f.conditionsAll,
							.conditionsNone = f.conditionsNone,
							.allFilters = allFilters });
						it = root.buckets.end() - 1;
					}
					it->mappings.push_back(mapping);
					return;
				}
				case TrieFillMode::kInclude:
				default: break;
			}

			auto toIDs = [](const std::vector<RE::FormID>& v) { return v.empty() ? std::vector<RE::FormID>{0x0} : v; };
			auto toEnum = [](const auto& v, auto any) {
				using T = std::decay_t<decltype(any)>;
				return v.empty() ? std::vector<T>{any} : v;
			};

			for (auto weapon : toIDs(f.weapons))
			for (auto weaponKeyword : toIDs(f.weaponKeywords))
			for (auto magicItem : toIDs(f.magicItems))
			for (auto projectile : toIDs(f.projectiles))
			for (auto magicEffect : toIDs(f.magicEffects))
			for (auto armor : toIDs(f.armors))
			for (auto armorKeyword : toIDs(f.armorKeywords))
			for (auto weaponType : toEnum(f.weaponTypes, Filter::WeaponType::kAny))
			for (auto armorClass : toEnum(f.armorClasses, Filter::ArmorClassType::kAny))
			for (auto blocked : toEnum(f.blockeds, Filter::BlockedFilter::kAny))
			for (auto state : toEnum(f.states, Filter::StateFilter::kAny))
			for (auto attack : toEnum(f.attacks, Filter::AttackFilter::kAny))
			for (auto source : toEnum(f.sources, Filter::SourceFilter::kAny))
			for (auto critical : toEnum(f.criticals, Filter::CriticalAttackFilter::kAny))
			for (auto sneak : toEnum(f.sneaks, Filter::SneakAttackFilter::kAny))
			for (auto bipedSlot : toSlots(f.bipedSlots))
			for (auto bipedLimb : toStrHashes(f.bipedLimbs))
			for (auto bipedNode : toStrHashes(f.bipedNodes))
			for (auto bipedKey : toStrHashes(f.bipedKeys)) {
				NodeType* node = &root;
				node = &node->children[TrieKey((uint8_t)TrieLevelHitContext::kWeapon, weapon)];
				node = &node->children[TrieKey((uint8_t)TrieLevelHitContext::kWeaponKeyword, weaponKeyword)];
				node = &node->children[TrieKey((uint8_t)TrieLevelHitContext::kMagicItem, magicItem)];
				node = &node->children[TrieKey((uint8_t)TrieLevelHitContext::kProjectile, projectile)];
				node = &node->children[TrieKey((uint8_t)TrieLevelHitContext::kMagicEffect, magicEffect)];
				node = &node->children[TrieKey((uint8_t)TrieLevelHitContext::kArmor, armor)];
				node = &node->children[TrieKey((uint8_t)TrieLevelHitContext::kArmorKeyword, armorKeyword)];
				node = &node->children[TrieKey((uint8_t)TrieLevelHitContext::kWeaponType, (uint32_t)weaponType)];
				node = &node->children[TrieKey((uint8_t)TrieLevelHitContext::kArmorClass, (uint32_t)armorClass)];
				node = &node->children[TrieKey((uint8_t)TrieLevelHitContext::kBlocked, (uint32_t)blocked)];
				node = &node->children[TrieKey((uint8_t)TrieLevelHitContext::kState, (uint32_t)state)];
				node = &node->children[TrieKey((uint8_t)TrieLevelHitContext::kAttack, (uint32_t)attack)];
				node = &node->children[TrieKey((uint8_t)TrieLevelHitContext::kSource, (uint32_t)source)];
				node = &node->children[TrieKey((uint8_t)TrieLevelHitContext::kCritical, (uint32_t)critical)];
				node = &node->children[TrieKey((uint8_t)TrieLevelHitContext::kSneak, (uint32_t)sneak)];
				node = &node->children[TrieKey((uint8_t)TrieLevelHitContext::kBipedSlot, bipedSlot)];
				node = &node->children[TrieKey((uint8_t)TrieLevelHitContext::kBipedLimb, bipedLimb)];
				node = &node->children[TrieKey((uint8_t)TrieLevelHitContext::kBipedNode, bipedNode)];
				node = &node->children[TrieKey((uint8_t)TrieLevelHitContext::kBipedKey, bipedKey)];

				auto it = std::ranges::find_if(node->buckets, [&](const auto& b) {
					return b.globalesAny == f.globalesAny &&
						 b.globalesAll == f.globalesAll &&
						 b.globalesNone == f.globalesNone &&
						 b.conditionsAny == f.conditionsAny &&
						 b.conditionsAll == f.conditionsAll &&
						 b.conditionsNone == f.conditionsNone &&
						 b.percentage == f.percentage &&
						 b.percentageMult == f.percentageMult &&
						 b.maxHealth == f.maxHealth &&
						 b.minDamage == f.minDamage &&
						 b.maxLimbHealth == f.maxLimbHealth;
				});
				if (it == node->buckets.end()) {
					node->buckets.push_back({ .idMapping = mapping->id,
						.globalesAny = f.globalesAny,
						.globalesAll = f.globalesAll,
						.globalesNone = f.globalesNone,
						.conditionsAny = f.conditionsAny,
						.conditionsAll = f.conditionsAll,
						.conditionsNone = f.conditionsNone,
						.percentage = f.percentage,
						.percentageMult = f.percentageMult,
						.maxHealth = f.maxHealth,
						.minDamage = f.minDamage,
						.maxLimbHealth = f.maxLimbHealth });
					it = node->buckets.end() - 1;
				}
				it->mappings.push_back(mapping);
			}
		}

		template <typename RegistryType, typename MappingType>
		static void RegisterImpactMapping(RegistryType& registry, MappingType* mapping, const ParsedMappingFilters& filters)
		{
			FillActorTrie(registry.victimInclude, filters.victimInc, mapping, TrieFillMode::kInclude);
			FillActorTrie(registry.victimExclude, filters.victimExc, mapping, TrieFillMode::kExclude);
			FillActorTrie(registry.victimInclude, filters.victimAll, mapping, TrieFillMode::kAll);
			FillActorTrie(registry.attackerInclude, filters.attackerInc, mapping, TrieFillMode::kInclude);
			FillActorTrie(registry.attackerExclude, filters.attackerExc, mapping, TrieFillMode::kExclude);
			FillActorTrie(registry.attackerInclude, filters.attackerAll, mapping, TrieFillMode::kAll);
			FillHitContextTrie(registry.hitContextInclude, filters.hitInc, mapping, TrieFillMode::kInclude);
			FillHitContextTrie(registry.hitContextExclude, filters.hitExc, mapping, TrieFillMode::kExclude);
			FillHitContextTrie(registry.hitContextInclude, filters.hitAll, mapping, TrieFillMode::kAll);
		}
	};
};
