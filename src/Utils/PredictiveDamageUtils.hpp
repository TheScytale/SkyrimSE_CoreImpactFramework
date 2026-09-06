#pragma once

// ============================================================================
// Custom replacement for the ActiveEffect::AdjustForPerks() method.
//
// The goal here is to compute the theoretical real magnitude/damage that a hit would deal
// taking into account every multiplicative and additive layer that the engine normally applies,
// without mutating the ActiveEffect/HitData object itself.
// ============================================================================

#include "DataHandler.hpp"

#include "Utils/MiscUtils.hpp"

class PredictiveDamageUtils
{
public:

	static float GetSneakAttackMult(RE::Actor* a_attacker, RE::Actor* a_victim, RE::TESObjectWEAP* a_weapon)
	{
		if (!a_attacker) return 1.0f;

		float baseSneakMult = 2.0f;
		if (a_weapon) {
			switch (a_weapon->GetWeaponType()) {
				case RE::WEAPON_TYPE::kHandToHandMelee:
					baseSneakMult = MiscUtils::GetGameSetting<float>("fCombatSneakHandMult", 2.0f);
					break;
				case RE::WEAPON_TYPE::kOneHandSword:
					baseSneakMult = MiscUtils::GetGameSetting<float>("fCombatSneak1HSwordMult", 2.0f);
					break;
				case RE::WEAPON_TYPE::kOneHandDagger:
					baseSneakMult = MiscUtils::GetGameSetting<float>("fCombatSneak1HDaggerMult", 2.0f);
					break;
				case RE::WEAPON_TYPE::kOneHandAxe:
					baseSneakMult = MiscUtils::GetGameSetting<float>("fCombatSneak1HAxeMult", 2.0f);
					break;
				case RE::WEAPON_TYPE::kOneHandMace:
					baseSneakMult = MiscUtils::GetGameSetting<float>("fCombatSneak1HMaceMult", 2.0f);
					break;
				case RE::WEAPON_TYPE::kTwoHandSword:
					baseSneakMult = MiscUtils::GetGameSetting<float>("fCombatSneak2HSwordMult", 2.0f);
					break;
				case RE::WEAPON_TYPE::kTwoHandAxe:
					baseSneakMult = MiscUtils::GetGameSetting<float>("fCombatSneak2HAxeMult", 2.0f);
					break;
				case RE::WEAPON_TYPE::kBow:
					baseSneakMult = MiscUtils::GetGameSetting<float>("fCombatSneakBowMult", 2.0f);
					break;
				case RE::WEAPON_TYPE::kCrossbow:
					baseSneakMult = MiscUtils::GetGameSetting<float>("fCombatSneakCrossbowMult", 2.0f);
					break;
				case RE::WEAPON_TYPE::kStaff:
					baseSneakMult = MiscUtils::GetGameSetting<float>("fCombatSneakStaffMult", 2.0f);
					break;
				default:
					baseSneakMult = MiscUtils::GetGameSetting<float>("fCombatSneakHandMult", 2.0f);
					break;
			}
		} else {
			baseSneakMult = MiscUtils::GetGameSetting<float>("fCombatSneakHandMult", 2.0f);
		}

		void* args[3] = {
			static_cast<void*>(a_attacker),
			static_cast<void*>(a_weapon),
			static_cast<void*>(a_victim)
		};

		const float perkSneakMult = AccumulateEntryPointMult(
			a_attacker, RE::BGSEntryPoint::ENTRY_POINT::kModSneakAttackMult,
			std::span(args), baseSneakMult);

		const float finalSneakMult = baseSneakMult * perkSneakMult;

		TRACE("[GetSneakAttackMult] attacker={} weapon={} victim={} base={:.4f} perkMult={:.4f} -> {:.4f}",
			a_attacker->GetName(), a_weapon ? a_weapon->GetName() : "null",
			a_victim ? a_victim->GetName() : "null", baseSneakMult, perkSneakMult, finalSneakMult);

		return finalSneakMult;
	}

	static float ComputePredictedArrowDamage(RE::Projectile* a_projectile, RE::Actor* a_attacker, RE::Actor* a_victim)
	{
		if (!a_projectile || !a_attacker) return 0.0f;

		auto& projectileRuntime = a_projectile->GetProjectileRuntimeData();
		if (!projectileRuntime.ammoSource) return 0.0f;

		const float baseDamage = projectileRuntime.weaponDamage;
		if (baseDamage <= 0.0f) return 0.0f;

		float avMult = 1.0f;
		if (auto* attackerAV = a_attacker->AsActorValueOwner()) {
			avMult = attackerAV->GetActorValue(RE::ActorValue::kAttackDamageMult);
		}

		RE::TESObjectWEAP* bow = projectileRuntime.weaponSource;

		float attackerPerkMult = 1.0f;
		{
			void* args[3] = {
				static_cast<void*>(a_attacker),
				static_cast<void*>(bow),
				static_cast<void*>(a_victim)
			};
			attackerPerkMult = AccumulateEntryPointMult(
				a_attacker, RE::BGSEntryPoint::ENTRY_POINT::kModAttackDamage,
				std::span(args), baseDamage * avMult);
		}

		float victimPerkMult = 1.0f;
		if (a_victim) {
			void* args[2] = {
				static_cast<void*>(a_victim),
				static_cast<void*>(a_attacker)
			};
			victimPerkMult = AccumulateEntryPointMult(
				a_victim, RE::BGSEntryPoint::ENTRY_POINT::kModIncomingDamage,
				std::span(args), baseDamage * avMult * attackerPerkMult);
		}

		const float predicted = baseDamage * avMult * attackerPerkMult * victimPerkMult;

		TRACE("[PredictArrowDamage] base={:.3f} avMult={:.3f} atkPerk={:.3f} vicPerk={:.3f} -> {:.3f}",
			baseDamage, avMult, attackerPerkMult, victimPerkMult, predicted);

		return predicted;
	}

	static float ComputeRealMagnitude(const RE::ActiveEffect* a_effect, RE::Actor* a_caster, RE::Actor* a_target,
		RE::ActorValue a_actorValue = RE::ActorValue::kHealth, float f_deltaTime = 1.0f)
	{
		if (!a_effect || !a_effect->effect || !a_effect->effect->baseEffect) return 0.0f;

		const float baseMagnitude = a_effect->effect->GetMagnitude();
		if (baseMagnitude <= 0.0f) return 0.0f;

		const float dualCastingModifier = a_effect->flags.any(RE::ActiveEffect::Flag::kDual)
			? MiscUtils::GetGameSetting<float>("fMagicDualCastingEffectivenessBase") : 1.0f;

		if (!a_target) return baseMagnitude;

		const auto* base = a_effect->GetBaseObject();
		RE::SpellItem* spell = a_effect->spell ? a_effect->spell->As<RE::SpellItem>() : nullptr;
		if (!a_target || !base || !spell) return baseMagnitude;

		RE::Actor* caster = a_caster;
		RE::Actor* target = a_target;

		float attackerPerkMult = 1.0f;
		if (caster) {
			void* args[3] = {
				static_cast<void*>(caster),
				static_cast<void*>(spell),
				static_cast<void*>(target)
			};
			attackerPerkMult = AccumulateEntryPointMult(
				caster, RE::BGSEntryPoint::ENTRY_POINT::kModSpellMagnitude,
				std::span(args), baseMagnitude);
		}

		float victimPerkMult = 1.0f;
		{
			void* args[2] = {
				static_cast<void*>(target),
				static_cast<void*>(spell)
			};
			victimPerkMult = AccumulateEntryPointMult(
				target, RE::BGSEntryPoint::ENTRY_POINT::kModIncomingSpellMagnitude,
				std::span(args), baseMagnitude * attackerPerkMult);
		}

		const float avMult = GetAVMagnitudeMult(target, base);

		float primaryContrib = base->data.primaryAV == a_actorValue ? 1.0f : 0.0f;
		float secondaryContrib = 0.0f;
		if (base->data.archetype == RE::EffectArchetype::kDualValueModifier && base->data.secondaryAV == a_actorValue) {
			secondaryContrib = base->data.secondAVWeight;
		}

		const float avContrib = primaryContrib + secondaryContrib;
		if (avContrib <= 0.0f) return 0.0f;

		const float adjusted = baseMagnitude * dualCastingModifier * attackerPerkMult * victimPerkMult * avMult * avContrib * f_deltaTime;

		TRACE("[ComputeRealMagnitude] effect={:08X} base={:.4f} atkPerk={:.4f} vicPerk={:.4f} av={:.4f} avContrib={:.4f} dt={:.4f} -> {:.4f}",
			 base->formID, baseMagnitude, attackerPerkMult, victimPerkMult, avMult, avContrib, f_deltaTime, adjusted);

		return adjusted;
	}

private:

	static float AccumulateEntryPointMult(RE::Actor* a_perkOwner, RE::BGSEntryPoint::ENTRY_POINT a_entryPoint,
		std::span<void*> a_condArgs, float a_currentMagnitude)
	{
		if (!a_perkOwner) return 1.0f;

		float additiveMult = 0.0f;
		float multiplicative = 1.0f;
		bool hasSetTo = false;
		float setValue = 0.0f;

		struct Visitor : RE::PerkEntryVisitor
		{
			std::span<void*> condArgs;
			float currentMagnitude;
			float& additiveMult;
			float& multiplicative;
			bool& hasSetTo;
			float& setValue;

			Visitor(std::span<void*> a_args, float a_curMag, float& a_add, float& a_mult, bool& a_hasSet, float& a_setVal)
				: condArgs(a_args), currentMagnitude(a_curMag),
				 additiveMult(a_add), multiplicative(a_mult), hasSetTo(a_hasSet), setValue(a_setVal)
			{}

			RE::BSContainer::ForEachResult Visit(RE::BGSPerkEntry* a_perkEntry) override
			{
				if (!a_perkEntry) return RE::BSContainer::ForEachResult::kContinue;

				auto* entryPoint = static_cast<RE::BGSEntryPointPerkEntry*>(a_perkEntry);
				const auto* perk = entryPoint->perk;
				if (!perk) return RE::BSContainer::ForEachResult::kContinue;

				if (!entryPoint->CheckConditionFilters(static_cast<int>(condArgs.size()), condArgs.data()))
					return RE::BSContainer::ForEachResult::kContinue;

				const auto* fnData = entryPoint->functionData;
				if (!fnData) return RE::BSContainer::ForEachResult::kContinue;

				const auto fnType = entryPoint->entryData.function.get();

				if (fnType == RE::BGSEntryPointPerkEntry::Function::kSetValue) {
					if (const auto* oneVal = skyrim_cast<const RE::BGSEntryPointFunctionDataOneValue*>(fnData)) {
						hasSetTo = true;
						setValue = oneVal->data;
						TRACE("[PerkAccum] perk={:08X} : SetValue={:.4f}", perk->formID, setValue);
					}
				} else if (fnType == RE::BGSEntryPointPerkEntry::Function::kAddValue) {
					if (const auto* oneVal = skyrim_cast<const RE::BGSEntryPointFunctionDataOneValue*>(fnData); oneVal && currentMagnitude > 0.0f) {
						additiveMult += oneVal->data / currentMagnitude;
						TRACE("[PerkAccum] perk={:08X} : AddValue={:.4f} (ratio={:.4f})", perk->formID, oneVal->data, oneVal->data / currentMagnitude);
					}
				} else if (fnType == RE::BGSEntryPointPerkEntry::Function::kMultiplyValue) {
					if (const auto* oneVal = skyrim_cast<const RE::BGSEntryPointFunctionDataOneValue*>(fnData)) {
						multiplicative *= oneVal->data;
						TRACE("[PerkAccum] perk={:08X} : MultiplyValue={:.4f}", perk->formID, oneVal->data);
					}
				}

				return RE::BSContainer::ForEachResult::kContinue;
			}
		};

		Visitor visitor(a_condArgs, a_currentMagnitude, additiveMult, multiplicative, hasSetTo, setValue);
		a_perkOwner->ForEachPerkEntry(a_entryPoint, visitor);

		if (hasSetTo) return a_currentMagnitude > 0.0f ? setValue / a_currentMagnitude : 1.0f;

		const float result = (1.0f + additiveMult) * multiplicative;
		return std::max(result, 0.0f);
	}

	static float GetAVMagnitudeMult(RE::Actor* a_target, const RE::EffectSetting* a_base)
	{
		if (!a_target || !a_base) return 1.0f;

		auto* avOwner = a_target->AsActorValueOwner();
		if (!avOwner) return 1.0f;

		float resistMagic = avOwner->GetActorValue(RE::ActorValue::kResistMagic);

		float resistSchool = 0.0f;
		switch (a_base->data.resistVariable) {
			case RE::ActorValue::kResistFire: resistSchool = avOwner->GetActorValue(RE::ActorValue::kResistFire); break;
			case RE::ActorValue::kResistFrost: resistSchool = avOwner->GetActorValue(RE::ActorValue::kResistFrost); break;
			case RE::ActorValue::kResistShock: resistSchool = avOwner->GetActorValue(RE::ActorValue::kResistShock); break;
			case RE::ActorValue::kResistDisease: resistSchool = avOwner->GetActorValue(RE::ActorValue::kResistDisease); break;
			case RE::ActorValue::kPoisonResist: resistSchool = avOwner->GetActorValue(RE::ActorValue::kPoisonResist); break;
			case RE::ActorValue::kResistMagic: resistSchool = 0.0f; break;
			default: break;
		}

		const float effectiveResist = std::clamp(resistMagic + resistSchool, -100.0f, 100.0f);
		const float mult = std::clamp(1.0f - effectiveResist * 0.01f, 0.0f, 2.0f);

		TRACE("[AVMult] target={} resistMagic={:.1f} resistSchool={:.1f} effective={:.1f} -> mult={:.4f}",
			 a_target->GetName(), resistMagic, resistSchool, effectiveResist, mult);

		return mult;
	}
};
