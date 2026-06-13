#include "domain/MedicineForm.h"

namespace MedicineForm {

const QVector<Group> &groups()
{
    static const QVector<Group> g = {
        {QStringLiteral("Solid orals"),
         {
             {QStringLiteral("TABLET"), QStringLiteral("Tablet")},
             {QStringLiteral("EFFERVESCENT_TABLET"), QStringLiteral("Effervescent tablet")},
             {QStringLiteral("CHEWABLE_TABLET"), QStringLiteral("Chewable tablet")},
             {QStringLiteral("DISPERSIBLE_TABLET"), QStringLiteral("Dispersible tablet")},
             {QStringLiteral("SUBLINGUAL_TABLET"), QStringLiteral("Sublingual tablet")},
             {QStringLiteral("LOZENGE"), QStringLiteral("Lozenge")},
             {QStringLiteral("CAPSULE"), QStringLiteral("Capsule")},
             {QStringLiteral("SOFT_GEL_CAPSULE"), QStringLiteral("Soft-gel capsule")},
             {QStringLiteral("ROTACAPS"), QStringLiteral("Rotacaps")},
         }},
        {QStringLiteral("Liquid orals"),
         {
             {QStringLiteral("SYRUP"), QStringLiteral("Syrup")},
             {QStringLiteral("DRY_SYRUP"), QStringLiteral("Dry syrup (reconstitutable)")},
             {QStringLiteral("SUSPENSION"), QStringLiteral("Suspension")},
             {QStringLiteral("ORAL_SOLUTION"), QStringLiteral("Oral solution")},
             {QStringLiteral("ELIXIR"), QStringLiteral("Elixir")},
             {QStringLiteral("EMULSION"), QStringLiteral("Emulsion")},
             {QStringLiteral("ORAL_DROPS"), QStringLiteral("Oral drops")},
         }},
        {QStringLiteral("Injectables"),
         {
             {QStringLiteral("INJECTION"), QStringLiteral("Injection (generic)")},
             {QStringLiteral("INJECTION_AMPOULE"), QStringLiteral("Injection — ampoule")},
             {QStringLiteral("INJECTION_VIAL"), QStringLiteral("Injection — vial")},
             {QStringLiteral("INJECTION_PREFILLED_SYRINGE"),
              QStringLiteral("Injection — prefilled syringe")},
             {QStringLiteral("INFUSION"), QStringLiteral("Infusion bag/bottle")},
             {QStringLiteral("IV_FLUID"), QStringLiteral("IV fluid")},
             {QStringLiteral("PEN_INJECTOR"), QStringLiteral("Pen injector")},
         }},
        {QStringLiteral("Inhalation"),
         {
             {QStringLiteral("INHALER"), QStringLiteral("Inhaler (generic)")},
             {QStringLiteral("INHALER_MDI"), QStringLiteral("Inhaler — MDI")},
             {QStringLiteral("INHALER_DPI"), QStringLiteral("Inhaler — DPI")},
             {QStringLiteral("NEBULIZER_SOLUTION"), QStringLiteral("Nebulizer solution")},
         }},
        {QStringLiteral("Drops & sprays"),
         {
             {QStringLiteral("DROPS"), QStringLiteral("Drops (generic)")},
             {QStringLiteral("EYE_DROPS"), QStringLiteral("Eye drops")},
             {QStringLiteral("EAR_DROPS"), QStringLiteral("Ear drops")},
             {QStringLiteral("NASAL_DROPS"), QStringLiteral("Nasal drops")},
             {QStringLiteral("NASAL_SPRAY"), QStringLiteral("Nasal spray")},
             {QStringLiteral("THROAT_SPRAY"), QStringLiteral("Throat spray")},
             {QStringLiteral("SPRAY"), QStringLiteral("Spray (generic)")},
         }},
        {QStringLiteral("Topicals"),
         {
             {QStringLiteral("CREAM"), QStringLiteral("Cream")},
             {QStringLiteral("OINTMENT"), QStringLiteral("Ointment")},
             {QStringLiteral("GEL"), QStringLiteral("Gel")},
             {QStringLiteral("LOTION"), QStringLiteral("Lotion")},
             {QStringLiteral("TOPICAL_SOLUTION"), QStringLiteral("Topical solution")},
             {QStringLiteral("MEDICATED_SOAP"), QStringLiteral("Medicated soap")},
             {QStringLiteral("SHAMPOO"), QStringLiteral("Medicated shampoo")},
         }},
        {QStringLiteral("Powders & sachets"),
         {
             {QStringLiteral("POWDER"), QStringLiteral("Powder")},
             {QStringLiteral("SACHET"), QStringLiteral("Sachet")},
             {QStringLiteral("DUSTING_POWDER"), QStringLiteral("Dusting powder")},
         }},
        {QStringLiteral("Local routes"),
         {
             {QStringLiteral("SUPPOSITORY"), QStringLiteral("Suppository")},
             {QStringLiteral("PESSARY"), QStringLiteral("Pessary")},
             {QStringLiteral("ENEMA"), QStringLiteral("Enema")},
         }},
        {QStringLiteral("Transdermal & implant"),
         {
             {QStringLiteral("PATCH"), QStringLiteral("Patch")},
             {QStringLiteral("TRANSDERMAL_PATCH"), QStringLiteral("Transdermal patch")},
             {QStringLiteral("IMPLANT"), QStringLiteral("Implant")},
         }},
        {QStringLiteral("Devices & misc"),
         {
             {QStringLiteral("DEVICE"), QStringLiteral("Device")},
             {QStringLiteral("TEST_STRIPS"), QStringLiteral("Test strips")},
             {QStringLiteral("MOUTHWASH"), QStringLiteral("Mouthwash")},
             {QStringLiteral("OTHER"), QStringLiteral("Other")},
         }},
    };
    return g;
}

QStringList allKeys()
{
    QStringList keys;
    for (const Group &grp : groups()) {
        for (const Option &o : grp.options) {
            keys << o.key;
        }
    }
    return keys;
}

bool isValid(const QString &key)
{
    return allKeys().contains(key);
}

QString label(const QString &key)
{
    for (const Group &grp : groups()) {
        for (const Option &o : grp.options) {
            if (o.key == key) {
                return o.label;
            }
        }
    }
    return key;
}

} // namespace MedicineForm
