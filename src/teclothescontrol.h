/*
 * teclothescontrol.h - the clothes tag list control and its helpers.
 *
 * Moved out of teeditor_derive.cpp, where the whole clothes editor used to be
 * declared inside the constructor of teEditor_clothes. Nothing but the name
 * changed: what was a local struct is a file scope class now.
 */
#pragma once
#include "teeditorhelpers.h"

// Clothes types, grouped by the body area they belong to. The groups are only a
// visual aid: every type button shares one exclusive group (built in
// teEditor_clothes), so a clothes tag still has exactly one type word.
//
// The vocabulary is docs/clothes_add_widget_v2.md §2, one list per area. It holds
// three kinds of word, all of them here for the same reason - filter() only ever
// asks "is the last word a type?":
//   * the word of a plain type button (B),
//   * the word of a button that also opens a menu (T) and every entry of that
//     menu (M), because a tag read back from a picture has to be recognised,
//   * capture-only words (C), which have no button at all.
// A multi-word type ("school uniform", "high tops", "mary janes") is one entry
// here and is written into a tag as one word - see clothesTagFromText().
//
// `clothes`, `top` and `sleeves` are deliberately absent: they are the three
// pure menu buttons and are not tags themselves (§1.6). Registering `top` or
// `sleeves` would also make "tank top" / "detached sleeves" look like
// derivations of a base word, which is exactly what §1.2 and §1.6 rule out.
inline QStringList headClothesTypes{
    qsl("hat"),qsl("beret"),qsl("witch hat"),qsl("mob cap"),qsl("baseball cap"),
    qsl("peaked cap"),qsl("top hat"),qsl("santa hat"),qsl("tokin hat"),qsl("sun hat"),
    qsl("beanie"),qsl("nurse cap"),qsl("straw hat"),qsl("newsboy cap"),qsl("sailor hat"),
    qsl("bonnet"),qsl("cowboy hat"),qsl("fedora"),qsl("jingasa"),qsl("deerstalker"),
    qsl("bucket hat"),qsl("ushanka"),qsl("mortarboard"),
    qsl("helmet"),qsl("hood"),
    qsl("crown"),qsl("tiara"),qsl("circlet"),qsl("headpiece"),qsl("head wreath"),
    qsl("laurel crown"),
    qsl("hairband"),
    qsl("headband"),qsl("forehead protector"),qsl("hachimaki"),qsl("sweatband"),
    qsl("veil"),qsl("bridal veil"),
    qsl("headdress"),qsl("ornament"),qsl("headscarf"),qsl("bandana"),qsl("balaclava"),
    qsl("coif"),qsl("ear covers"),qsl("wig"),
    // bows and ribbons (§2.2): both are base words of their own family
    qsl("bow"),qsl("hair bow"),qsl("back bow"),qsl("ear bow"),qsl("bow choker"),
    qsl("bow legwear"),qsl("chest bow"),qsl("wrist bow"),
    qsl("ribbon"),qsl("hair ribbon"),qsl("neck ribbon"),qsl("tress ribbon"),
    qsl("leg ribbon"),qsl("arm ribbon"),qsl("thigh ribbon"),qsl("footwear ribbon"),
    // hair ornaments (§2.3)
    qsl("hair ornament"),qsl("hairclip"),qsl("hair flower"),qsl("hair tubes"),
    qsl("hair bobbles"),qsl("hair scrunchie"),qsl("hair tie"),qsl("hair stick"),
    qsl("kanzashi"),qsl("hair beads"),
    // eyes, face, neck, shoulders (§2.4)
    qsl("glasses"),qsl("sunglasses"),qsl("goggles"),qsl("monocle"),qsl("pince-nez"),
    qsl("mask"),qsl("surgical mask"),
    qsl("headphones"),qsl("headset"),qsl("earphones"),qsl("earpiece"),
    qsl("earrings"),qsl("hoop earrings"),qsl("stud earrings"),qsl("earclip"),
    qsl("ear chain"),
    qsl("collar"),qsl("sailor collar"),qsl("detached collar"),qsl("wing collar"),
    qsl("high collar"),qsl("fur collar"),qsl("mandarin collar"),qsl("neck ruff"),
    qsl("usekh collar"),
    qsl("choker"),qsl("neck bell"),qsl("spiked collar"),qsl("ribbon choker"),
    qsl("o-ring choker"),qsl("pendant choker"),
    qsl("necktie"),qsl("ascot"),qsl("cross tie"),qsl("bolo tie"),
    qsl("bowtie"),qsl("neckerchief"),qsl("scarf"),
    qsl("necklace"),qsl("brooch"),qsl("pendant"),
    qsl("tie clip"),qsl("badge"),
    qsl("cape"),qsl("capelet"),qsl("cloak"),qsl("hooded cloak"),qsl("waist cape"),
    qsl("side cape"),
    qsl("shawl"),qsl("stole"),qsl("epaulettes"),qsl("lapels"),qsl("aiguillette"),
    qsl("feather boa")
};
inline QStringList upperBodyClothesTypes{
    qsl("shirt"),qsl("collared shirt"),qsl("sleeveless shirt"),qsl("dress shirt"),
    qsl("t-shirt"),qsl("blouse"),qsl("frilled shirt"),qsl("off-shoulder shirt"),
    qsl("crop top"),qsl("halterneck"),qsl("tank top"),qsl("camisole"),qsl("bandeau"),
    qsl("tube top"),qsl("underbust"),qsl("bustier"),
    qsl("turtleneck"),
    qsl("sweater"),qsl("cardigan"),qsl("sweater vest"),qsl("turtleneck sweater"),
    qsl("ribbed sweater"),qsl("virgin killer sweater"),qsl("aran sweater"),
    qsl("jacket"),qsl("blazer"),qsl("cropped jacket"),qsl("suit jacket"),
    qsl("leather jacket"),qsl("letterman jacket"),qsl("bomber jacket"),
    qsl("denim jacket"),qsl("sukajan"),
    qsl("coat"),qsl("tailcoat"),qsl("long coat"),qsl("winter coat"),qsl("trench coat"),
    qsl("raincoat"),qsl("fur coat"),qsl("duffel coat"),qsl("overcoat"),
    qsl("hoodie"),
    qsl("vest"),qsl("waistcoat"),
    qsl("apron"),
    qsl("robe"),qsl("open robe"),qsl("bathrobe"),
    qsl("poncho"),qsl("tabard"),qsl("surcoat"),qsl("tunic"),qsl("shrug (clothing)"),
    qsl("corset"),qsl("muneate")
};
inline QStringList lowerBodyClothesTypes{
    qsl("pants"),qsl("jeans"),qsl("yoga pants"),qsl("capri pants"),qsl("sweatpants"),
    qsl("lowleg pants"),qsl("bell-bottoms"),qsl("detached pants"),
    qsl("shorts"),qsl("short shorts"),qsl("bike shorts"),qsl("denim shorts"),
    qsl("micro shorts"),qsl("dolphin shorts"),qsl("shorts under skirt"),
    qsl("gym shorts"),qsl("pleated shorts"),
    qsl("bloomers"),qsl("buruma"),
    qsl("pelvic curtain"),qsl("petticoat"),qsl("sarong"),qsl("chaps"),
    qsl("skirt"),qsl("pleated skirt"),qsl("miniskirt"),qsl("plaid skirt"),
    qsl("long skirt"),qsl("high-waist skirt"),qsl("suspender skirt"),
    qsl("microskirt"),qsl("showgirl skirt"),qsl("bubble skirt"),qsl("overskirt"),
    qsl("tutu"),qsl("overall skirt"),
    qsl("overalls")
};
inline QStringList legwearClothesTypes{
    qsl("thighhighs"),qsl("over-kneehighs"),qsl("kneehighs"),
    qsl("socks"),qsl("tabi"),qsl("loose socks"),qsl("bobby socks"),qsl("ankle socks"),
    qsl("pantyhose"),qsl("fishnet pantyhose"),qsl("thighband pantyhose"),
    qsl("pantyhose under shorts"),
    qsl("legwear"),qsl("leggings"),qsl("leg warmers")
};
inline QStringList feetClothesTypes{
    qsl("shoes"),qsl("sneakers"),qsl("loafers"),qsl("mary janes"),
    qsl("platform shoes"),qsl("flats"),qsl("dress shoes"),qsl("ballet slippers"),
    qsl("high tops"),qsl("converse"),
    qsl("boots"),qsl("thigh boots"),qsl("knee boots"),qsl("high heel boots"),
    qsl("lace-up boots"),qsl("ankle boots"),qsl("armored boots"),
    qsl("platform boots"),qsl("rubber boots"),qsl("cowboy boots"),
    qsl("high heels"),qsl("pumps"),qsl("stiletto heels"),qsl("platform heels"),
    qsl("wedge heels"),
    qsl("sandals"),qsl("geta"),qsl("platform sandals"),qsl("zouri"),
    qsl("flip-flops"),qsl("okobo"),qsl("gladiator sandals"),
    qsl("slippers"),qsl("uwabaki"),qsl("crocs"),qsl("animal slippers"),
    qsl("heels"),
    // Older pictures say "footwear", and genericClothesTypes below still has to be
    // able to absorb it into shoes/boots: a capture-only word, no button (§2.14).
    qsl("footwear")
};
inline QStringList fullBodyClothesTypes{
    qsl("dress"),qsl("sailor dress"),qsl("pinafore dress"),qsl("wedding dress"),
    qsl("sundress"),qsl("halter dress"),qsl("sweater dress"),qsl("nightgown"),
    qsl("evening gown"),
    qsl("serafuku"),
    qsl("school uniform"),qsl("gakuran"),qsl("gym uniform"),
    qsl("meiji schoolgirl uniform"),
    qsl("suit"),qsl("business suit"),qsl("skirt suit"),qsl("tuxedo"),qsl("pant suit"),
    qsl("armor"),qsl("bikini armor"),qsl("armored dress"),
    qsl("military uniform"),qsl("cheerleader"),
    qsl("uniform"),qsl("costume"),
    qsl("bodysuit"),qsl("mecha pilot suit"),qsl("bodystocking"),qsl("plugsuit"),
    qsl("diving suit"),qsl("bikesuit"),qsl("wetsuit"),
    qsl("leotard"),qsl("playboy bunny"),qsl("strapless leotard"),
    qsl("see-through leotard"),
    qsl("swimsuit"),qsl("bikini"),qsl("one-piece swimsuit"),qsl("school swimsuit"),
    qsl("competition swimsuit"),qsl("slingshot swimsuit"),qsl("tankini"),
    qsl("rash guard"),qsl("swim briefs"),
    qsl("side-tie bikini bottom"),qsl("string bikini"),qsl("micro bikini"),
    qsl("criss-cross halter"),qsl("thong bikini"),qsl("lowleg bikini"),
    qsl("sports bikini"),
    qsl("jumpsuit"),qsl("romper"),qsl("unitard"),qsl("harem outfit"),qsl("babydoll"),
    qsl("kigurumi"),qsl("loincloth"),qsl("kesa"),qsl("cassock")
};
inline QStringList traditionalClothesTypes{
    qsl("japanese clothes"),qsl("chinese clothes"),qsl("greco-roman clothes"),
    qsl("korean clothes"),qsl("arabian clothes"),qsl("ancient egyptian clothes"),
    qsl("hakama"),qsl("sarashi"),qsl("haori"),qsl("tabi"),qsl("fundoshi"),
    qsl("tasuki"),qsl("happi"),qsl("kimono skirt"),
    qsl("kimono"),qsl("yukata"),qsl("short kimono"),qsl("furisode"),
    qsl("layered kimono"),qsl("uchikake"),
    qsl("qipao"),qsl("hanfu"),qsl("tangzhuang"),qsl("changpao"),
    qsl("hanbok"),qsl("ao dai"),qsl("dirndl")
};
inline QStringList accessoryClothesTypes{
    // sleeves (§2.11) - `sleeves` itself is a pure menu button, not a type
    qsl("long sleeves"),qsl("short sleeves"),qsl("puffy sleeves"),qsl("wide sleeves"),
    qsl("frilled sleeves"),qsl("juliet sleeves"),qsl("sleeves rolled up"),
    qsl("layered sleeves"),qsl("sleeve cuffs"),
    qsl("detached sleeves"),qsl("arm warmers"),qsl("single detached sleeve"),
    qsl("raglan sleeves"),
    // hands and arms (§2.12)
    qsl("gloves"),qsl("elbow gloves"),qsl("fingerless gloves"),qsl("bridal gauntlets"),
    qsl("mittens"),qsl("boxing gloves"),qsl("yugake"),
    qsl("bracelet"),qsl("bangle"),qsl("spiked bracelet"),
    qsl("ring"),qsl("wedding ring"),
    qsl("wrist cuffs"),qsl("wrist scrunchie"),qsl("wristband"),qsl("armband"),
    qsl("armlet"),qsl("bracer"),qsl("arm guards"),qsl("knee pads"),qsl("anklet"),
    qsl("thighlet"),
    // waist and torso (§2.13)
    qsl("belt"),
    qsl("sash"),qsl("obi"),qsl("waist sash"),qsl("shoulder sash"),
    qsl("suspenders"),
    qsl("garter"),qsl("garter straps"),qsl("garter belt"),
    qsl("thigh strap"),qsl("frilled thigh strap"),
    qsl("harness"),
    // underwear (§2.14)
    qsl("panties"),qsl("bra"),qsl("lingerie"),qsl("sports bra"),qsl("pasties"),
    qsl("boxer briefs"),
    // "underwear" is the word the panties button deliberately does *not* use, but
    // genericClothesTypes still absorbs it into panties - old tags keep working
    // and stay editable (§2.14). Capture only, no button.
    qsl("underwear"),
    // non-garment types older pictures still rely on (§2.16.1); `bag` and
    // `animal ears` moved to the accessory list below when they became buttons
    qsl("wings"),qsl("horns"),
    // bags, halos, tails and animal ears (§2.16.2). They are worn, so they are
    // accessories with their own menu - and every menu entry is a type of its
    // own, which is what makes "red cat tail" keep `cat tail` in one piece.
    qsl("bag"),qsl("randoseru"),qsl("school bag"),qsl("school briefcase"),
    qsl("halo"),
    qsl("tail"),qsl("cat tail"),qsl("fox tail"),qsl("wolf tail"),qsl("dog tail"),
    qsl("horse tail"),qsl("devil tail"),qsl("dragon tail"),qsl("fish tail"),
    qsl("bird tail"),
    qsl("ears"),qsl("animal ears"),qsl("cat ears"),qsl("rabbit ears"),
    qsl("fox ears"),qsl("dog ears"),qsl("wolf ears"),qsl("horse ears"),
    qsl("pointy ears"),
    // a cut-out is a state of a garment, but it is captured like a type (§2.15)
    qsl("clothing cutout")
};

/// Every word the clothes editor recognises as a garment type.
inline QStringList allClothesTypes = []{
    QStringList types;
    for(const QStringList* group : {&headClothesTypes,&upperBodyClothesTypes,
                                    &lowerBodyClothesTypes,&legwearClothesTypes,
                                    &feetClothesTypes,&fullBodyClothesTypes,
                                    &traditionalClothesTypes,&accessoryClothesTypes})
        types.append(*group);
    // A word may sit in two areas (tabi, swim briefs): one type is enough.
    types.removeDuplicates();
    return types;
}();

/**
 * @brief How a type is spelled inside a tag: one word, or one word per part.
 *
 * A type whose *last* word is a type of its own (`sailor collar`: `collar` is a
 * type) is a modifier in front of that type, so its words stay separate. Welding
 * them together is what made `pink glowing halo` impossible to bring back to
 * `pink halo`: the menu could no longer see `glowing` as its own word, and the
 * next pick put a second `glowing` in front of it.
 *
 * A type whose last word is not a type (`crop top`, `long sleeves`: `top` and
 * `sleeves` alone are not garment types) is a compound and stays ONE word - split
 * it and the tag reads as an adjective plus a word that means nothing.
 */
inline QStringList clothesTypeWords(const QString& type)
{
    const QStringList words = type.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (words.size() < 2)
        return words;
    if (allClothesTypes.contains(words.back()))
        return words;
    return QStringList{type};
}

/// Generic garment words the user does not want, and the specific words they are
/// absorbed by. A tag that says "underwear" or "footwear" is merged into an
/// existing tag of the same family, and the *specific* spelling survives.
inline QHash<QString,QSet<QString>> genericClothesTypes{
    {qsl("underwear"),{qsl("panties")}},
    {qsl("footwear"),{qsl("shoes"),qsl("boots")}},
};

/**
 * @brief The tag `text` describes, with the type spelled the way a tag spells it.
 *
 * The type keeps the spelling it was written with (`clothesTypeWords`): "crop
 * top" stays one word because `top` alone is not a garment type, while "sailor
 * collar" is `sailor` + `collar` because `collar` is one - welding it would take
 * the modifier away from the word list, and nothing could remove it again.
 * "red crop top" therefore becomes "red" + "crop top", and "red sailor collar"
 * becomes "red" + "sailor" + "collar".
 *
 * Everything else goes through teTag(text), exactly like a hand-typed tag.
 */
inline std::shared_ptr<teTag> clothesTagFromText(const QString& text)
{
    QString type;
    for (const QString& candidate : allClothesTypes) {
        if (!candidate.contains(QLatin1Char(' ')))
            continue;
        if (candidate.size() <= type.size())
            continue;
        if (text == candidate || text.endsWith(QLatin1Char(' ') + candidate))
            type = candidate;
    }
    if (type.isEmpty())
        return std::make_shared<teTag>(text);
    auto tag = std::make_shared<teTag>();
    const QString head = text.left(text.size() - type.size()).trimmed();
    for (const QString& word : head.split(QLatin1Char(' '), Qt::SkipEmptyParts))
        tag->words.append(new teWord{word});
    for (const QString& word : clothesTypeWords(type))
        tag->words.append(new teWord{word});
    return tag;
}

/// True when the two type words mean the same garment, i.e. may not both exist in
/// one tag list. A generic word matches every specific word of its family.
inline bool sameClothesType(const QString& a,const QString& b){
    if(a==b)
        return true;
    for(auto it=genericClothesTypes.constBegin();it!=genericClothesTypes.constEnd();++it){
        if(it.key()==a&&it.value().contains(b))
            return true;
        if(it.key()==b&&it.value().contains(a))
            return true;
    }
    return false;
}

/// True when `type` is one of the generic words (which never survive a merge).
inline bool isGenericClothesType(const QString& type){
    return genericClothesTypes.contains(type);
}
inline std::multimap<QString, QString> prefix_back{
    {qsl("dress"),qsl("wedding")},
    {qsl("waist"),qsl("apron")},
    {qsl("bikini"),qsl("one-piece")},
    {qsl("swimsuit"),qsl("one-piece")},
    {qsl(""),qsl("fishnet")},
    {qsl(""),qsl("sleeveless")},
    { qsl("dress"),qsl("china") },
    // trims that read as "<type> with <trim>", i.e. they follow the type in the
    // canonical order (§2.15)
    {qsl(""),qsl("fur trim")},
    {qsl(""),qsl("ribbon trim")},
    {qsl(""),qsl("gold trim")},
    {qsl(""),qsl("lace trim")},
    {qsl(""),qsl("white trim")},
    {qsl(""),qsl("silver trim")},
    {qsl(""),qsl("flower trim")}
};
inline std::multimap<QString, QString> prefix_front{
    {qsl(""),qsl("torn")},
    {qsl(""),qsl("detached")},
    {qsl(""),qsl("unworn")},
    {qsl(""),qsl("short")},
    {qsl(""),qsl("long")},
    {qsl(""),qsl("frilled")},
    // garment details that stand in front of the type (§2.15)
    {qsl(""),qsl("naked")},
    {qsl(""),qsl("open")},
    {qsl(""),qsl("wet")},
    {qsl(""),qsl("see-through")},
    {qsl(""),qsl("backless")},
    {qsl(""),qsl("crotchless")},
    {qsl(""),qsl("fur-trimmed")},
    {qsl(""),qsl("lace-trimmed")},
    {qsl(""),qsl("ribbon-trimmed")},
    {qsl(""),qsl("gold-trimmed")},
    {qsl(""),qsl("double-breasted")},
    {qsl(""),qsl("criss-cross straps")},
    {qsl(""),qsl("frills")},
    {qsl(""),qsl("gathers")},
    {qsl(""),qsl("buttons")},
    {qsl(""),qsl("tassel")},
    {qsl(""),qsl("o-ring")},
    {qsl(""),qsl("buckle")},
    {qsl(""),qsl("zipper")},
    {qsl(""),qsl("dress flower")},
    {qsl(""),qsl("chinese knot")},
    {qsl(""),qsl("pentacle")},
    {qsl(""),qsl("medallion")}
};
/// Groups of words that may not appear together in one clothes tag.
inline QVector<QSet<QString>> exclusiveModifierGroups{

};

/// Prepositions that turn a tag into an interaction ("reaching through
/// panties", "hand in panties") instead of a piece of clothing.
///
/// This is deliberately a *small* list: it is the exclusion dictionary asked
/// for, and a word such as "with" is missing on purpose so that legitimate
/// garment tags ("dress with bow") keep working.

/// True when a word before the trailing type word is a preposition, i.e. the
/// tag describes what someone does with a garment rather than the garment.
inline bool looksLikeActionPhrase(const teTag& tag){
    const int last = tag.words.size()-1;
    for(int i=0;i<last;++i)
        if(preposwords.contains(tag.words[i]->text))
            return true;
    return false;
}

/**
 * @brief Which part of a clothes tag a word is.
 *
 * The clothes editor spends most of its logic on this classification: a tag is
 * "colours + front adjectives + adjectives + type", and the canonical order of
 * those parts is what the editor rewrites a tag into.
 */
enum class ClothesRole { Colour, FrontAdjective, Adjective, BackAdjective, Type };

/**
 * @brief True when `adj` is listed for `type` (or for every type) in `map`.
 *
 * Pure lookup; the maps are prefix_front / prefix_back below.
 */
inline bool findInPrefixMap(const std::multimap<QString,QString>& map,
                            const QString& type,const QString& adj){
    auto range = map.equal_range(type);
    for(auto it=range.first;it!=range.second;++it)
        if(adj==it->second)
            return true;
    auto general = map.equal_range(qsl(""));
    for(auto it=general.first;it!=general.second;++it)
        if(adj==it->second)
            return true;
    return false;
}

/**
 * @brief The role of word `index` of a clothes tag.
 *
 * This is the per word form: readCore() walks a tag word by word and calls this,
 * so nothing is allocated on that path. classifyClothesWords() is the whole tag
 * form used by the regression harness.
 *
 * prefix_back is honoured here (it was not, until the vocabulary owner asked for
 * it: the lookup used to always query prefix_front, so words such as "wedding"
 * or "one-piece" never reached back_adjectives and a tag like "wedding lace
 * dress" kept its word order instead of being sorted to "lace wedding dress").
 */
inline ClothesRole clothesRoleAt(const teTag& tag,int index,const QString& type,
                                 const QPair<int,int>& colorSlot){
    if(index>=colorSlot.first&&index<colorSlot.first+colorSlot.second)
        return ClothesRole::Colour;
    if(index>=0&&index<tag.words.size()){
        const QString& text = tag.words[index]->text;
        if(findInPrefixMap(prefix_front,type,text))
            return ClothesRole::FrontAdjective;
        if(findInPrefixMap(prefix_back,type,text))
            return ClothesRole::BackAdjective;
    }
    return ClothesRole::Adjective;
}

/**
 * @brief The role of every word of `tag`, in tag order (last one is the type).
 *
 * Pure: it reads the tag and the prefix tables and nothing else, so the clothes
 * rules can finally be checked in the regression harness - they used to be
 * reachable only by opening the window and looking at it.
 */
inline QVector<ClothesRole> classifyClothesWords(const teTag& tag){
    QVector<ClothesRole> roles;
    if(tag.words.isEmpty())
        return roles;
    roles.reserve(tag.words.size());
    const QString type = tag.words.back()->text;
    const auto colorSlot = is_color(tag);
    const int count = tag.words.size()-1;
    for(int i=0;i<count;++i)
        roles.push_back(clothesRoleAt(tag,i,type,colorSlot));
    roles.push_back(ClothesRole::Type);
    return roles;
}

/**
 * @brief A clothes tag split into the parts the editor reasons about.
 *
 * "colours + front adjectives + adjectives + back adjectives + type" is the
 * canonical order a clothes tag is rewritten into, and every question the editor
 * asks about a tag (may these two merge, what would the merged text be) is
 * answered from this - computed on demand instead of kept in a per-tag object
 * that had to be deleted at exactly the right moment.
 */
struct ClothesParts {
    QStringList colors;
    QStringList front;
    QStringList adjectives;
    QStringList back;
    QString type;
};

/**
 * @brief The garment type a tag ends with, as one unit.
 *
 * A type may be more than one word ("fingerless gloves", "school uniform",
 * "pleated skirt"). The add widget stores it as a single word, but a hand-typed
 * or hand-merged tag can spell it as two - and then the *longest* registered
 * suffix is the type. Reading only the last word made `fingerless` an adjective,
 * so adding a modifier put it between the two words ("fingerless fishnet
 * gloves") and the tag degraded to a plain `gloves`: the button that wrote it no
 * longer claimed it and the type could not be edited as one thing any more.
 */
inline QString clothesTypeSuffix(const teTag& tag)
{
    if (tag.words.isEmpty())
        return QString();
    const QString text = static_cast<QString>(tag);
    QString best;
    for (const QString& candidate : allClothesTypes) {
        if (candidate.size() <= best.size())
            continue;
        if (text == candidate || text.endsWith(QLatin1Char(' ') + candidate))
            best = candidate;
    }
    return best.isEmpty() ? tag.words.back()->text : best;
}

/**
 * @brief Splits `tag` into its parts. Pure: it reads the tag and the tables.
 *
 * The per-word rule is clothesRoleAt(), so this is the whole-tag form of the
 * classification the control makes while it walks the words.
 */
inline ClothesParts clothesParse(const teTag& tag)
{
    ClothesParts parts;
    if (tag.words.isEmpty())
        return parts;
    parts.type = clothesTypeSuffix(tag);
    // How many *words* of this tag spell the type. Both spellings exist - one
    // word (`crop top`, see clothesTypeWords) or one word per part (`sailor
    // collar`, and every hand-typed tag) - and counting the parts of the spelling
    // instead of the words of the tag made the adjective count 2-2 = 0 for
    // `red crop top`, which dropped the colour from the canonical words.
    int typeWordCount = 0;
    {
        QString joined;
        for (int i = tag.words.size() - 1; i >= 0; --i) {
            joined = joined.isEmpty() ? tag.words[i]->text
                                      : tag.words[i]->text + QLatin1Char(' ') + joined;
            ++typeWordCount;
            if (joined == parts.type)
                break;
            if (!parts.type.endsWith(joined)) {
                typeWordCount = 0;          // the type is not a suffix of this tag
                break;
            }
        }
        if (typeWordCount <= 0)
            typeWordCount = qMin(parts.type.split(QLatin1Char(' '), Qt::SkipEmptyParts).size(),
                                 int(tag.words.size()));
    }
    const int adjectiveCount = qMax(0, tag.words.size() - typeWordCount);
    const auto colorSlot = is_color(tag);
    for (int i = 0; i < adjectiveCount; ++i) {
        const QString& text = tag.words[i]->text;
        QStringList* bucket = nullptr;
        switch (clothesRoleAt(tag, i, parts.type, colorSlot)) {
        case ClothesRole::Colour:         bucket = &parts.colors;     break;
        case ClothesRole::FrontAdjective: bucket = &parts.front;      break;
        case ClothesRole::BackAdjective:  bucket = &parts.back;       break;
        case ClothesRole::Adjective:
        case ClothesRole::Type:           bucket = &parts.adjectives; break;
        }
        if (bucket && !bucket->contains(text))
            bucket->append(text);
    }
    return parts;
}

/// The words of `parts` in canonical order, each word only once.
inline QStringList clothesCanonicalWords(const ClothesParts& parts)
{
    QStringList words;
    words.reserve(parts.colors.size()+parts.front.size()
                  +parts.adjectives.size()+parts.back.size()+1);
    for (const QStringList* bucket : {&parts.colors,&parts.front,&parts.adjectives,&parts.back})
        for (const QString& text : *bucket)
            if (!words.contains(text))
                words.append(text);
    words += clothesTypeWords(parts.type);
    return words;
}

/**
 * @brief True when `incoming` may be merged into a tag that looks like `target`.
 *
 * The garment has to be the same (sameClothesType handles the generic words) and
 * the colours have to agree: "red dress" and "blue dress" stay apart, "red
 * dress" and "red torn dress" merge. Pure, so the merge *rules* - which used to
 * be reachable only by typing two tags into the window - can be checked.
 */
inline bool clothesMergeable(const ClothesParts& target,const teTag& incoming)
{
    if(target.type.isEmpty()||incoming.words.isEmpty())
        return false;
    if(!sameClothesType(clothesTypeSuffix(incoming),target.type))
        return false;
    const auto [colorpos,colorcount] = is_color(incoming);
    if(colorcount>0&&!target.colors.isEmpty()){
        if(colorcount!=target.colors.size())
            return false;
        for(int i=0;i<colorcount;++i)
            if(incoming.words[i+colorpos]->text!=target.colors[i])
                return false;
    }
    return true;
}

/// Convenience for a target known only by its garment word and its colours.
inline bool clothesMergeable(const QString& type,const QStringList& colors,const teTag& incoming)
{
    ClothesParts target;
    target.type = type;
    target.colors = colors;
    return clothesMergeable(target,incoming);
}

/**
 * @brief The words of a tag that has absorbed `incoming`.
 *
 * The absorbing tag keeps its garment and its colours; the incoming tag
 * contributes its adjectives, and a generic garment word ("underwear",
 * "footwear") gives way to the specific spelling the incoming tag has
 * ("panties", "boots"). Pure, so what a merge produces can be checked without
 * opening the window.
 */
inline QStringList clothesMergedWords(const ClothesParts& target,const teTag& incoming)
{
    ClothesParts merged = target;
    if(!incoming.words.isEmpty()){
        const QString incomingType = clothesTypeSuffix(incoming);
        if(isGenericClothesType(merged.type)&&!isGenericClothesType(incomingType))
            merged.type = incomingType;
    }
    const ClothesParts in = clothesParse(incoming);
    if(merged.colors.isEmpty())
        merged.colors = in.colors;
    merged.front += in.front;
    merged.adjectives += in.adjectives;
    merged.back += in.back;
    return clothesCanonicalWords(merged);
}

/**
 * @brief Writes `words` into `tag`, reusing the word objects that survive.
 *
 * Words are matched by text and moved, so a widget showing one keeps its
 * connection; a word that is no longer part of the tag is deleted (after the
 * widgets have let go of it) and a new text gets a new word object. `widget` is
 * the tag's widget, or nullptr when the control does not show one. The change is
 * announced once - edited_with_layout() - which is what makes every list showing
 * the tag reload its text.
 */
inline void clothesApplyWords(std::shared_ptr<teTag> tag,const QStringList& words,
                              teTagWidgetBase* widget)
{
    if(!tag)
        return;
    QMultiHash<QString,teWord*> available;
    for(teWord* word : tag->words)
        if(word)
            available.insert(word->text,word);
    QVector<teWord*> next;
    next.reserve(words.size());
    for(const QString& text : words){
        auto it = available.find(text);
        if(it==available.end())
            next.append(new teWord{text});
        else{
            next.append(*it);
            available.erase(it);
        }
    }
    const bool hasWidget = widget!=nullptr;
    if(hasWidget)
        widget->disconnectWord();
    tag->words.clear();
    for(teWord* leftover : available){      // duplicates the canonical list dropped
        if(hasWidget)
            widget->forgetWord(leftover);
        delete leftover;
    }
    int pos = -1;
    for(teWord* word : next){
        if(hasWidget)
            widget->insertWord(++pos,word,true,false);
        else
            tag->words.append(word);
    }
    tag->edited_with_layout();
}

/// Words that make a clothes tag a *state* rather than a garment.
inline const QSet<QString>& clothesStateModifiers(){
    static const QSet<QString> words{
        qsl("no"),qsl("without"),qsl("unworn"),qsl("open"),
        // How the garment looks worn, rather than which garment it is (§2.15):
        // "see-through clothes", "torn pantyhose", "naked apron" stay garments
        // with an adjective, so they may be edited but never merged.
        qsl("see-through"),qsl("torn"),qsl("wet"),qsl("backless"),
        qsl("crotchless"),qsl("naked"),qsl("revealing")
    };
    return words;
}

/**
 * @brief True when the tag carries such a modifier.
 *
 * "no shoes", "top without sleeves", "unworn panties", "open jacket" describe a
 * state, so they must not take part in a clothes merge in either direction - the
 * same idea as the action phrases ("... through panties") that filter() keeps out
 * of the control. Unlike those, these tags *are* clothes tags and stay editable;
 * they are only excluded from merging.
 */
/**
 * @brief 以衣物类型结尾、但根本不归衣物控件管的 tag。
 *
 * `anal tail` 是 Objects 菜单里的东西(尾巴形状的玩具),可它的末尾正好是
 * 配饰类型 `tail`,只按后缀判定会被衣物控件认领,Objects 按钮就永远点不亮。
 * 这类"撞名"逐个列出来,比让 `tail` 猜词性安全。
 */
inline bool clothesIsFalseFriend(const teTag& tag)
{
    static const QSet<QString> set{qsl("anal tail")};
    return set.contains(static_cast<QString>(tag));
}

inline bool clothesHasStateModifier(const teTag& tag){
    for(const teWord* word : tag.words)
        if(word&&clothesStateModifiers().contains(word->text))
            return true;
    return false;
}

/**
 * @brief 裸基 tag:除了衣物类型词没有任何其它成分("dress")。
 *
 * "red dress" 不是(有颜色),"elaborate frilled dress" 也不是(有修饰词)。
 * 多人图只允许与裸基 tag 自动合并——两条都带描述的衣物可能属于不同的人。
 * Pure。
 */
inline bool isBareClothesTag(const teTag& tag)
{
    if(tag.words.isEmpty())
        return false;
    const ClothesParts parts=clothesParse(tag);
    return parts.colors.isEmpty()&&parts.front.isEmpty()
          &&parts.adjectives.isEmpty()&&parts.back.isEmpty();
}

/**
 * @brief The clothes control.
 *
 * It used to own one `teClothes` object per tag: that object cached the parsed
 * tag, held a shared_ptr to it and kept two connections on it, and the whole
 * lifetime machine around it (a `dead` flag, a retired list and a deferred purge
 * timer) existed only to keep the object from being used after it had been
 * deleted - and it still leaked one per tag per picture switch, which kept whole
 * tag lists alive.
 *
 * None of that state was necessary. Everything a wrapper knew is a pure function
 * of the tag (clothesParse), so the control keeps nothing but the tags it linked
 * and recomputes on demand. filter() is a predicate again: the merge it used to
 * perform from inside is the explicit absorb() step in read()/onTagEdited().
 */
struct ClothesList : public teTagListControl {
    /// The tag this control is rewriting right now.
    ///
    /// Its own announcement must not send it through the merge/rewrite path
    /// again; the wrapper had the same guard and called it clothes_editing.
    std::shared_ptr<teTag> clothes_editing = nullptr;
    /**
     * @brief The linked clothes tags, in link order.
     *
     * When two tags could absorb a third one, the first one linked wins - the
     * insertion order the old type-sorted multiset gave. Only the order matters:
     * the tags are owned by the tag list and the shared_ptrs here are dropped in
     * unlink(), exactly like linked_tags.
     */
    QVector<std::shared_ptr<teTag>> clothes_order;

    ClothesList(colorsWidget*in_onEdit_widget,TagListView*parentlist,QWidget*parent = nullptr,QString*styleSheet=nullptr)
        :teTagListControl(in_onEdit_widget,parentlist,parent,styleSheet,"clothes"){
        this->info=QStringLiteral("ClothesList");

    }

    /// 本列表提供右键 merge(Tab / Shift+M)。
    bool manualMergeSupported() const override { return true; }

    /**
     * @brief Is this tag a garment? Pure.
     *
     * It changes nothing: the merge, the insertion and the retirement that used
     * to happen in here are absorb() now, called from the two places that really
     * want them (read() and onTagEdited()). That is what makes the answer to
     * "is this mine?" worth something.
     */
    bool filter(std::shared_ptr<teTag> in_tag) const override{
        if(!in_tag||in_tag->words.empty())
            return false;
        if(in_tag==clothes_editing)
            return true;                        // our own rewrite, still ours
        // "reaching through panties" and friends are actions, not garments.
        if(looksLikeActionPhrase(*in_tag))
            return false;
        if(clothesIsFalseFriend(*in_tag))
            return false;
        return allClothesTypes.contains(clothesTypeSuffix(*in_tag));
    }

    /**
     * @brief The add widget produced `data`.
     *
     * A multi-word type has to reach the tag list as one word, or the tag read
     * back from the picture is not the one the button stands for (see
     * clothesTagFromText). Everything else - including an edit of the selected
     * tag - stays the base class's business.
     */
    void reciveWidgetSignal(QString data,bool ifadd)override{
        if(ifedit||!taglistwidget){
            teTagListControl::reciveWidgetSignal(data,ifadd);
            return;
        }
        taglistwidget->tagInsertAbove(false,clothesTagFromText(data),true);
    }

    TagHandling read(std::shared_ptr<teTag> tag)override{
        if(!tag)
            return TagHandling::NotMine;
        const bool mine = filter(tag);
        if(tag->retired){
            if(taglistwidget)
                taglistwidget->retire(tag);
            return TagHandling::Consumed;
        }
        if(!mine)
            return TagHandling::NotMine;
        if(absorb(tag)){
            if(taglistwidget)
                taglistwidget->retire(tag);
            return TagHandling::Consumed;
        }
        if(taglistwidget)
            taglistwidget->ensureWidgetFor(tag);
        link(tag);
        if(tag->retired){
            if(taglistwidget)
                taglistwidget->retire(tag);
            return TagHandling::Consumed;
        }
        return TagHandling::Linked;
    }

    /**
     * @brief The tag was edited: absorb it, or bring it back into shape.
     *
     * This replaces the wrapper's reReadClothes(), which had to find its own
     * wrapper by pointer first and to live with being deleted from inside the
     * signal it was called from.
     */
    void onTagEdited(std::shared_ptr<teTag> tag){
        if(!tag||tag->retired)
            return;
        if(tag==clothes_editing)
            return;                             // our own rewrite
        if(!linked_tags.count(tag))
            return;                             // not one of ours (any more)
        if(!filter(tag)){
            unlink(tag);
            return;
        }
        if(absorb(tag)){
            if(taglistwidget)
                taglistwidget->retire(tag);
            return;
        }
        // No re-sorting here. An edit is the user's word order - dragging a word
        // in the ref list has to stick, and the add widget already writes the
        // canonical order itself. Re-canonicalising on every edit is what made
        // the list feel order-locked (and what tore a multi-word type apart).
        // Merges still produce canonical order: clothesMergedWords() sorts, and
        // that path does not go through here.
    }

    /**
     * @brief Merges `incoming` into the first linked tag that accepts it.
     *
     * Returns true when it did, and the caller then retires `incoming` (marked by
     * this call). The absorbing tag is rewritten through applyWords(), so the
     * result is announced like any other edit.
     */
    bool absorb(std::shared_ptr<teTag> incoming){
        if(!incoming||incoming->words.isEmpty())
            return false;
        if(!autoMerge||!MergeSwitch)
            return false;
        if(!allClothesTypes.contains(clothesTypeSuffix(*incoming)))
            return false;
        // A tag that describes a state ("no shoes", "unworn panties") neither
        // absorbs another clothes tag nor is absorbed by one - in either
        // direction.
        if(clothesHasStateModifier(*incoming))
            return false;
        // 多人图(girl+boy+other > 1)只在一边是裸基 tag 时自动合并:
        // "red dress" + "dress" 还是同一条裙子,而 "elaborate frilled dress"
        // 和 "white dress" 可能属于两个人,不再自动合并。手动合并走列表
        // 右键的 merge(Tab / Shift+M)。
        const bool multiPeople = isMultiPeople(taglistwidget?taglistwidget->showing_list:nullptr);
        for(const std::shared_ptr<teTag>& target : clothes_order){
            if(!target||target==incoming||target->retired)
                continue;
            if(!linked_tags.count(target))
                continue;
            if(multiPeople&&!isBareClothesTag(*target)&&!isBareClothesTag(*incoming))
                continue;
            if(mergeInto(target,incoming))
                return true;
        }
        return false;
    }

    /**
     * @brief 把 `incoming` 合并进指定的 `target`。
     *
     * absorb() 的实际合并步骤。`manual` = 用户显式要求(列表右键的 merge /
     * Tab / Shift+M):不再问 autoMerge / MergeSwitch 与 mergeSuppressed——
     * 这是用户亲手点的;markMerged 照常执行,撤销照样能整步还原。
     */
    bool mergeInto(std::shared_ptr<teTag> target,std::shared_ptr<teTag> incoming,bool manual=false){
        if(!target||!incoming||target==incoming||target->retired||incoming->retired)
            return false;
        if(!linked_tags.count(target))
            return false;
        if(clothesHasStateModifier(*target)||clothesHasStateModifier(*incoming))
            return false;
        const ClothesParts parts=clothesParse(*target);
        if(parts.type.isEmpty())
            return false;
        if(!clothesMergeable(parts,*incoming))
            return false;
        if(!manual&&!mergeAllowed(*target,*incoming))
            return false;
        const QStringList words=clothesMergedWords(parts,*incoming);
        markMerged(*target,*incoming);
        incoming->retired=true;
        writeWords(target,words);
        return true;
    }

    /**
     * @brief `anchor` 在列表里最近的、与它可合并的邻居(向上或向下找)。
     *
     * 没有时返回 nullptr。纯查找,不改任何东西——手动合并的级联用它逐跳
     * 推进;谁幸存由级联处决定(向上优先上面的幸存)。
     */
    std::shared_ptr<teTag> findManualMergeNeighbour(
        const std::shared_ptr<teTag>& anchor,bool up)
    {
        if(!layout||!anchor)
            return {};
        const int anchorIndex=findindex(anchor);
        const int count=layout->count();
        if(anchorIndex<0)
            return {};
        for(int step=1;step<count;++step){
            const int index=up?anchorIndex-step:anchorIndex+step;
            if(index<0||index>=count)
                break;                          // 走出了这一端
            auto* widget=dynamic_cast<teRefTagWidget*>(layout->itemAt(index)->widget());
            if(!widget||!widget->core||widget->core->retired)
                continue;
            const std::shared_ptr<teTag> candidate=widget->core;
            if(candidate==anchor)
                continue;
            // 同一衣物、颜色相容即算可合并(clothesMergeable 对称)。
            const ClothesParts candidateParts=clothesParse(*candidate);
            const ClothesParts anchorParts=clothesParse(*anchor);
            if(candidateParts.type.isEmpty()||anchorParts.type.isEmpty())
                continue;
            if(clothesMergeable(candidateParts,*anchor)||clothesMergeable(anchorParts,*candidate))
                return candidate;
        }
        return {};
    }

    /**
     * @brief 手动合并(列表右键 merge / Tab / Shift+M)。
     *
     * 多选:从上到下链式合并——第一个 tag 是幸存者,后面的逐条尝试并进
     * 它,合不进去的保持独立(成为新的幸存者)。单选:级联——从该 tag 出发,
     * 先向上找最近的可合并邻居并进去(上面的幸存),再向下找,循环到两头
     * 都找不到为止(用户给的例子:red gradient hair 先并进上方的 white hair,
     * 得到的 white to red gradient hair 再并进下方的 blue gradient hair)。
     */
    void manualMerge()override{
        if(!taglistwidget||!taglistwidget->showing_list)
            return;
        const QVector<std::shared_ptr<teTag>> chosen=selectedCoresInVisualOrder();
        if(chosen.isEmpty())
            return;
        if(chosen.size()>=2){
            QVector<std::shared_ptr<teTag>> survivors;
            for(const std::shared_ptr<teTag>& tag:chosen){
                if(!tag||tag->retired)
                    continue;
                bool absorbed=false;
                for(const std::shared_ptr<teTag>& survivor:survivors){
                    if(mergeInto(survivor,tag,true)){
                        taglistwidget->retire(tag);
                        absorbed=true;
                        break;
                    }
                }
                if(!absorbed)
                    survivors.append(tag);
            }
            return;
        }
        // 单选:级联,先向上后向下。anchor 始终是"活着的这条 tag"。
        std::shared_ptr<teTag> anchor=chosen.first();
        for(int round=0;round<64&&anchor&&!anchor->retired;++round){
            std::shared_ptr<teTag> neighbour=findManualMergeNeighbour(anchor,true);
            if(neighbour&&mergeInto(neighbour,anchor,true)){
                taglistwidget->retire(anchor);
                anchor=neighbour;
                continue;
            }
            if(neighbour&&mergeInto(anchor,neighbour,true)){
                taglistwidget->retire(neighbour);
                continue;
            }
            neighbour=findManualMergeNeighbour(anchor,false);
            if(neighbour&&mergeInto(anchor,neighbour,true)){
                taglistwidget->retire(neighbour);
                continue;
            }
            if(neighbour&&mergeInto(neighbour,anchor,true)){
                taglistwidget->retire(anchor);
                anchor=neighbour;
                continue;
            }
            break;                                  // 两头都没有可合并的邻居
        }
    }

    /// Puts one tag's words into canonical order (no merge, no widget churn when
    /// the tag already is canonical).
    void applyWords(std::shared_ptr<teTag> tag){
        writeWords(tag,clothesCanonicalWords(clothesParse(*tag)));
    }

    /**
     * @brief The reftag menu picked `tags` for `tag`: swap the garment's type.
     *
     * This is §1.13's "drag a reftag onto a menu entry". The menu stands for one
     * *spelling* of the type (`family`: the longest one this tag ends with -
     * `glowing halo` on `pink glowing halo`, `collar` on `red collar`), and the
     * pick replaces exactly that spelling: `pink glowing halo` + `halo` gives
     * `pink halo`, `red collar` + `sailor collar` gives `red sailor collar`.
     * Using the registered type instead of the spelling kept `glowing` (it is not
     * a type on its own) and put the picked spelling after it.
     *
     * Applying the entry to the button's capture list, which is what used to
     * happen, found nothing to replace at all: `red collar` is not in the collar
     * menu, so it simply wrote a second tag next to it.
     *
     * The picked spelling is written the way a tag spells it
     * (`clothesTypeWords`): `sailor collar` becomes `sailor` + `collar`, so the
     * modifier can be taken away again, while `crop top` stays one word.
     *
     * `writeWords` is the internal rewrite path, so the change is one undo step
     * and does not send the tag through absorb() - composing a spelling is not
     * the same as editing a tag into existence, and a merge here would undo the
     * choice the user just made.
     */
    bool retag(std::shared_ptr<teTag> tag,const QString& family,const QStringList& tags){
        if(!tag||tag->words.isEmpty()||tags.size()!=1||family.isEmpty())
            return false;
        const QString wanted=tags.front();
        if(wanted.isEmpty()||!filter(tag))
            return false;
        if(family==wanted)
            return true;                        // already this spelling
        const QString current=static_cast<QString>(*tag);
        if(!(current==family||current.endsWith(QLatin1Char(' ')+family)))
            return false;                       // the spelling is not in this tag
        QStringList words=current.left(current.size()-family.size())
                              .split(QLatin1Char(' '),Qt::SkipEmptyParts);
        words+=clothesTypeWords(wanted);
        writeWords(tag,words);
        // The same duplicate rule an edited tag gets (teTagList::edit): the one
        // the user just renamed wins over an identical twin - the tag list may
        // already hold the spelling that was picked.
        if(taglistwidget&&taglistwidget->showing_list)
            taglistwidget->showing_list->remove_duplicate(tag,true);
        return true;
    }

    /**
     * @brief Writes `words` into `tag` and announces it.
     *
     * Nothing is written when the tag already spells them: an announcement that
     * changes nothing is not harmless here - it makes every list reload, and it
     * is what would turn "already in order" into an endless loop of re-reads.
     */
    void writeWords(std::shared_ptr<teTag> tag,const QStringList& words){
        if(!tag||tag->words.isEmpty())
            return;
        QStringList current;
        current.reserve(tag->words.size());
        for(const teWord* word : tag->words)
            current.append(word->text);
        if(current==words)
            return;
        // Another control reading this tag must not start merging while the words
        // are being replaced (the wrapper suppressed it the same way).
        const bool previousSwitch = MergeSwitch;
        MergeSwitch = false;
        const std::shared_ptr<teTag> previousEditing = clothes_editing;
        clothes_editing = tag;
        clothesApplyWords(tag,words,tagWidgetFor(tag));
        clothes_editing = previousEditing;
        MergeSwitch = previousSwitch;
    }

    void link(std::shared_ptr<teTag> in_tag)override{
        linked_tags.insert(in_tag);
        clothes_order.append(in_tag);
        in_tag->teConnect(teCallbackType::destroy,this,&EditorControl::unlink,in_tag);
        // Canonical order first, before the widget exists: that is the order the
        // old wrapper read the tag in, so the widget is built from the final words
        // instead of being rewritten right after it was created.
        applyWords(in_tag);
        in_tag->teConnect(teCallbackType::edit,this,&ClothesList::onTagEdited,in_tag);
        in_tag->teConnect(teCallbackType::edit_with_layout,this,&ClothesList::onTagEdited,in_tag);
        teTagWidgetBase*widget =taginsert(-1,in_tag,taglistwidget->showing_list);
        extraWidgetPushBack(in_tag,widget);
    }

    void unlink(std::shared_ptr<teTag> in_tag)override{
        teDisconnect(in_tag.get());
        clothes_order.removeAll(in_tag);
        linked_tags.erase(in_tag);
        tagErase(in_tag);
    }

    void resetPolicy()override{
        clothes_order.clear();
        clothes_editing.reset();
    }

    void extraWidgetPushBack(std::shared_ptr<teTag>in_tag,teTagWidgetBase*widget){
        if(in_tag->words.empty()||*in_tag->words.back()!=qsl("bow"))
            return;
        int wordcount = in_tag->words.size();
        QPushButton*hairbow_btn = new QPushButton("hair",widget);
        hairbow_btn->setStyleSheet(qsl(R"(font:8pt "Sonsolas")"));
        hairbow_btn->setCheckable(true);
        hairbow_btn->setFixedHeight(15);
        hairbow_btn->setContentsMargins(2,1,2,1);
        for(int i =0;i<wordcount;++i)
            if(*in_tag->words[i]==qsl("hair"))
                hairbow_btn->setChecked(true);
        connect(hairbow_btn,&QPushButton::clicked,widget,[in_tag,this](bool ifchecked){
            auto* editorTag=tagWidgetFor(in_tag);
            if(ifchecked){
                // Never a second "hair": clicking again must not add another
                // word (it used to, so "hair" piled up).
                if(in_tag->contains(qsl("hair")))
                    return;
                if(editorTag)
                    editorTag->insertWord(-2,qsl("hair"));
            }else{
                // Every "hair" is removed *without* signalling in between.
                // Each destroyWord() re-enters this control through
                // edited_with_layout() -> onTagEdited() -> applyWords(), which
                // rewrites the tag's words - so the old loop walked a vector that
                // had already been rewritten, and the word either survived or came
                // back. One announcement at the end is enough.
                bool removed=false;
                for(int i=in_tag->words.size()-1;i>=0;--i)
                    if(*in_tag->words[i]==qsl("hair")){
                        if(editorTag)
                            editorTag->destroyWord(i,false);
                        else{
                            delete in_tag->words[i];
                            in_tag->words.erase(in_tag->words.begin()+i);
                        }
                        removed=true;
                    }
                if(removed)
                    in_tag->edited_with_layout();
            }
        },Qt::DirectConnection);
        widget->insertExtraWidgets(this,hairbow_btn);
    }
};
