#include "KesProject.h"
#include "KeyScanner.h"
#include "Languages.h"
#include "LocProject.h"
#include "PoFile.h"
#include "po_catalog.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <cstdio>

using namespace kle;

namespace {

int g_failures = 0;

#define EXPECT(cond)                                                       \
    do {                                                                   \
        if (!(cond)) {                                                     \
            std::printf("FAILED line %d: %s\n", __LINE__, #cond);          \
            ++g_failures;                                                  \
        }                                                                  \
    } while (0)

const char* kPolish =
    "# Polish translation\n"
    "# Second comment line\n"
    "msgid \"\"\n"
    "msgstr \"\"\n"
    "\"Language: pl\\n\"\n"
    "\"Content-Type: text/plain; charset=UTF-8\\n\"\n"
    "\"Plural-Forms: nplurals=3; plural=(n==1 ? 0 : n%10>=2 && n%10<=4 && (n%100<10 || n%100>=20) ? 1 : 2);\\n\"\n"
    "\n"
    "# Translator note\n"
    "#. Shown on the title screen\n"
    "#: ui/menu.rml:4\n"
    "msgid \"MainMenu.NewGame\"\n"
    "msgstr \"Nowa gra\"\n"
    "\n"
    "msgid \"Inventory.Apples\"\n"
    "msgid_plural \"Inventory.Apples\"\n"
    "msgstr[0] \"{0} jabłko\"\n"
    "msgstr[1] \"{0} jabłka\"\n"
    "msgstr[2] \"{0} jabłek\"\n"
    "\n"
    "#, fuzzy\n"
    "msgid \"Intro.Text\"\n"
    "msgstr \"\"\n"
    "\"Linia pierwsza\\n\"\n"
    "\"Linia \\\"druga\\\"\"\n"
    "\n"
    "msgctxt \"menu\"\n"
    "msgid \"With.Context\"\n"
    "msgstr \"Z kontekstem\"\n"
    "\n"
    "#~ msgid \"Old.Thing\"\n"
    "#~ msgstr \"Stare\"\n";

void writeFile(const QString& path, const QByteArray& data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    (void)f.open(QIODevice::WriteOnly);
    f.write(data);
}

QString readFile(const QString& path)
{
    QFile f(path);
    (void)f.open(QIODevice::ReadOnly);
    return QString::fromUtf8(f.readAll());
}

void testRoundTrip()
{
    PoFile po;
    QString error;
    EXPECT(parsePo(QString::fromUtf8(kPolish), po, &error));
    EXPECT(error.isEmpty());
    EXPECT(po.hasHeader);
    EXPECT(po.headerValue("language") == "pl");
    EXPECT(po.pluralCount() == 3);
    EXPECT(po.headerComments.size() == 2);
    EXPECT(po.entries.size() == 5);

    const int newGame = po.findEntry("MainMenu.NewGame");
    EXPECT(newGame == 0);
    EXPECT(po.entries[newGame].translatorComments == QStringList{"Translator note"});
    EXPECT(po.entries[newGame].extractedComments == QStringList{"Shown on the title screen"});
    EXPECT(po.entries[newGame].references == QStringList{"ui/menu.rml:4"});
    EXPECT(po.entries[newGame].translations == QStringList{"Nowa gra"});

    const PoEntry& apples = po.entries[po.findEntry("Inventory.Apples")];
    EXPECT(apples.plural);
    EXPECT(apples.translations.size() == 3);
    EXPECT(apples.translations[2] == "{0} jabłek");

    const PoEntry& intro = po.entries[po.findEntry("Intro.Text")];
    EXPECT(intro.isFuzzy());
    EXPECT(intro.translations == QStringList{"Linia pierwsza\nLinia \"druga\""});
    EXPECT(!intro.isTranslated());

    EXPECT(po.entries[3].isPassthrough());
    EXPECT(po.entries[4].isPassthrough());
    EXPECT(po.entries[4].raw.contains("#~ msgstr \"Stare\""));

    const QString written = writePo(po);
    PoFile again;
    EXPECT(parsePo(written, again, &error));
    EXPECT(writePo(again) == written);
    EXPECT(written.contains("#. Shown on the title screen\n#: ui/menu.rml:4\nmsgid \"MainMenu.NewGame\"\nmsgstr \"Nowa gra\""));
    EXPECT(written.contains("msgid \"Inventory.Apples\"\nmsgid_plural \"Inventory.Apples\"\nmsgstr[0]"));
    EXPECT(written.contains("msgstr \"\"\n\"Linia pierwsza\\n\"\n\"Linia \\\"druga\\\"\"\n"));
    EXPECT(written.contains("msgctxt \"menu\"\nmsgid \"With.Context\""));

    PoCatalog catalog;
    EXPECT(catalog.Parse(written.toStdString()));
    EXPECT(catalog.Find("MainMenu.NewGame") && *catalog.Find("MainMenu.NewGame") == "Nowa gra");
    EXPECT(catalog.FindPlural("Inventory.Apples", 5) && *catalog.FindPlural("Inventory.Apples", 5) == "{0} jabłek");
}

void testParseErrors()
{
    PoFile po;
    QString error;
    EXPECT(!parsePo("msgid \"\"\nmsgstr \"\"\nmsgid \"K\"\nmsgstr \"b\n", po, &error));
    EXPECT(error.startsWith("line 4"));
    EXPECT(!parsePo("msgid \"K\"\n", po, &error));
    EXPECT(!parsePo("bogus \"x\"\n", po, &error));
}

void testProject()
{
    QTemporaryDir tmp;
    const QString dir = tmp.path() + "/localization";
    writeFile(dir + "/en.po",
              "msgid \"\"\nmsgstr \"\"\n\"Language: en\\n\"\n\n"
              "msgid \"MainMenu.NewGame\"\nmsgstr \"New Game\"\n\n"
              "msgid \"MainMenu.Quit\"\nmsgstr \"\"\n");
    writeFile(dir + "/pl.po", kPolish);

    LocProject project;
    QStringList errors;
    EXPECT(project.load(dir, "en", &errors));
    EXPECT(errors.isEmpty());
    EXPECT(project.languageCodes() == (QStringList{"en", "pl"}));
    EXPECT(project.keys() == (QStringList{"Intro.Text", "Inventory.Apples", "MainMenu.NewGame", "MainMenu.Quit"}));

    EXPECT(project.status("pl", "MainMenu.NewGame") == CellStatus::Translated);
    EXPECT(project.status("pl", "Intro.Text") == CellStatus::Fuzzy);
    EXPECT(project.status("pl", "MainMenu.Quit") == CellStatus::Missing);
    EXPECT(project.status("en", "Inventory.Apples") == CellStatus::Missing);
    EXPECT(project.status("en", "MainMenu.Quit") == CellStatus::Untranslated);
    EXPECT(project.translatedCount("pl") == 2);

    project.setTranslation("pl", "MainMenu.Quit", {"Wyjdź"});
    EXPECT(project.status("pl", "MainMenu.Quit") == CellStatus::Translated);
    EXPECT(!project.entry("pl", "MainMenu.Quit")->plural);

    project.setTranslation("en", "MainMenu.NewGame", {"Start New Game"});
    EXPECT(project.status("pl", "MainMenu.NewGame") == CellStatus::Fuzzy);
    EXPECT(project.status("pl", "MainMenu.Quit") == CellStatus::Translated);
    project.setFuzzy("pl", "MainMenu.NewGame", false);
    EXPECT(project.status("pl", "MainMenu.NewGame") == CellStatus::Translated);

    EXPECT(project.addKey("#HUD.Score", "Score: {{score}}"));
    EXPECT(project.addKey("Hud.Kills", "{0} kill", "{0} kills"));
    EXPECT(project.entry("en", "Hud.Kills")->translations == (QStringList{"{0} kill", "{0} kills"}));
    project.setTranslation("pl", "Hud.Kills", {"{0} zabójstwo"});
    EXPECT(project.entry("pl", "Hud.Kills")->plural);
    EXPECT(project.entry("pl", "Hud.Kills")->translations.size() == 3);
    project.setPlural("Hud.Kills", false);
    EXPECT(project.entry("en", "Hud.Kills")->translations == QStringList{"{0} kill"});
    EXPECT(project.entry("pl", "Hud.Kills")->translations == QStringList{"{0} zabójstwo"});
    project.setPlural("Hud.Kills", true);
    EXPECT(project.entry("pl", "Hud.Kills")->translations.size() == 3);
    EXPECT(!project.addKey("HUD.Score", "dup"));
    EXPECT(!project.addKey("1Bad", "x"));
    EXPECT(project.status("pl", "HUD.Score") == CellStatus::Missing);

    EXPECT(project.renameKey("HUD.Score", "Hud.Score"));
    EXPECT(!project.hasKey("HUD.Score") && project.hasKey("#Hud.Score"));
    project.setNote("Hud.Score", "Top right corner");
    EXPECT(project.entry("en", "Hud.Score")->extractedComments == QStringList{"Top right corner"});

    QString error;
    EXPECT(project.addLanguage("de-de", &error));
    EXPECT(project.languageCodes() == (QStringList{"en", "de_DE", "pl"}));
    EXPECT(project.entry("de_DE", "MainMenu.Quit") != nullptr);
    EXPECT(project.status("de_DE", "MainMenu.Quit") == CellStatus::Untranslated);
    EXPECT(!project.addLanguage("pl", &error));
    EXPECT(!project.addLanguage("../x", &error));

    project.removeKey("Intro.Text");
    EXPECT(!project.hasKey("Intro.Text"));

    const QStringList written = project.save(&error);
    if (!error.isEmpty()) std::printf("save: %s\n", qPrintable(error));
    EXPECT(error.isEmpty());
    EXPECT(written.size() == 3);
    EXPECT(project.save(&error).isEmpty());

    PoCatalog pl;
    EXPECT(pl.Parse(readFile(dir + "/pl.po").toStdString()));
    EXPECT(pl.Find("MainMenu.Quit") && *pl.Find("MainMenu.Quit") == "Wyjdź");
    EXPECT(!pl.FindEntry("Intro.Text"));
    EXPECT(pl.GetSkippedCount() == 1);
    PoCatalog en;
    EXPECT(en.Parse(readFile(dir + "/en.po").toStdString()));
    EXPECT(en.Find("Hud.Score") && *en.Find("Hud.Score") == "Score: {{score}}");
    EXPECT(!en.FindPlural("Hud.Kills", 2));   // the plural text was dropped by setPlural(false)
    EXPECT(readFile(dir + "/pl.po").startsWith("# Polish translation\n# Second comment line\nmsgid \"\""));
    EXPECT(readFile(dir + "/pl.po").contains("#~ msgid \"Old.Thing\""));
    EXPECT(!readFile(dir + "/pl.po").contains("msgid \"New Game\""));
    EXPECT(readFile(dir + "/de_DE.po").contains("Plural-Forms: nplurals=2; plural=(n != 1);"));
}

void testNewSourceFile()
{
    QTemporaryDir tmp;
    LocProject project;
    EXPECT(project.load(tmp.path() + "/localization", "en"));
    EXPECT(project.languageCodes() == QStringList{"en"});
    QString error;
    EXPECT(project.save(&error).isEmpty());   // an empty new file isn't written
    EXPECT(project.addKey("A.B", "Hello"));
    EXPECT(project.save(&error).size() == 1);
    EXPECT(QFile::exists(tmp.path() + "/localization/en.po"));
}

void testScanner()
{
    ScanResult r;
    const QStringList functions = {"Tr", "TrPlural", "LocalizationHasKey"};
    scanText("auto a = Tr(\"#MainMenu.NewGame\");\n"
             "auto b = TrPlural( \"Inventory.Apples\", n);\n"
             "auto c = MyTr(\"#Not.This\");\n"
             "auto d = Tr(someVariable);\n"
             "LocalizationHasKey(u8\"#Hud\");\n",
             ScanFileKind::Cpp, "src/main.cpp", functions, r);
    EXPECT(r.references.contains("MainMenu.NewGame"));
    EXPECT(r.references["MainMenu.NewGame"].first().line == 1);
    EXPECT(r.references.contains("Inventory.Apples"));
    EXPECT(r.references["Inventory.Apples"].first().line == 2);
    EXPECT(!r.references.contains("Not.This"));
    EXPECT(r.references.contains("Hud"));
    EXPECT(r.references.size() == 3);

    ScanResult rml;
    scanText("<rml><head><title>#Menu.Title</title></head>\n<body><p>\n  #Menu.Body\n</p>"
             "<p>Press #Input.Start now</p><p id=\"#Not.Text\">#1 Player</p><p>#Short</p></body></rml>",
             ScanFileKind::Rml, "ui/menu.rml", functions, rml);
    EXPECT(rml.references.contains("Menu.Title"));
    EXPECT(rml.references.contains("Menu.Body"));
    EXPECT(rml.references["Menu.Body"].first().line == 3);
    EXPECT(rml.references.contains("Short"));
    EXPECT(!rml.references.contains("Input.Start"));   // the engine translates whole text nodes only
    EXPECT(!rml.references.contains("Not.Text"));
    EXPECT(rml.references.size() == 3);

    ScanResult text;
    scanText("Entity { Message=\"#Dialog.Guard.Hello\" Color=#ffffff Url=\"http://x/#Not.Key\" }\n"
             "End of sentence #Story.End. Next ##Not.Two &#Not.Three",
             ScanFileKind::Text, "maps/a.fmap", functions, text);
    EXPECT(text.references.contains("Dialog.Guard.Hello"));
    EXPECT(text.references.contains("Story.End"));
    EXPECT(text.references.size() == 2);

    EXPECT(scanFileKind("a/b.HPP") == ScanFileKind::Cpp);
    EXPECT(scanFileKind("menu.rml") == ScanFileKind::Rml);
    EXPECT(scanFileKind("menu.rcss") == ScanFileKind::Skip);
    EXPECT(scanFileKind("pl.po") == ScanFileKind::Skip);
    EXPECT(scanFileKind("level.fmap") == ScanFileKind::Text);
}

void testScanFolders()
{
    QTemporaryDir tmp;
    const QString root = tmp.path();
    writeFile(root + "/game.kproject", "Project {\n    AssetRoot=\"content\"\n}\n");
    writeFile(root + "/src/main.cpp", "Tr(\"#Code.Key\");\n");
    writeFile(root + "/content/ui/menu.rml", "<p>#Ui.Key</p>");
    writeFile(root + "/content/localization/en.po", "msgctxt \"Po.Key\"\nmsgid \"x\"\nmsgstr \"\"\n# #Ignored.Key\n");
    writeFile(root + "/out/build/gen.cpp", "Tr(\"#Build.Key\");\n");
    writeFile(root + "/content/blob.dat", QByteArray("#Binary.Key\0\0", 13));

    const ProjectPaths paths = findProject(root + "/content/ui");
    EXPECT(paths.isValid());
    EXPECT(QDir(paths.projectDir) == QDir(root));
    EXPECT(QDir(paths.localizationDir) == QDir(root + "/content/localization"));

    ScanSettings settings;
    settings.baseFolder = paths.projectDir;
    settings.folders = {paths.projectDir};
    const ScanResult r = scanForKeys(settings);
    EXPECT(r.references.contains("Code.Key"));
    EXPECT(r.references["Code.Key"].first().file == "src/main.cpp");
    EXPECT(r.references.contains("Ui.Key"));
    EXPECT(!r.references.contains("Ignored.Key"));
    EXPECT(!r.references.contains("Build.Key"));
    EXPECT(!r.references.contains("Binary.Key"));
}

void testLanguages()
{
    EXPECT(normalizeLanguageCode("pt-br") == "pt_BR");
    EXPECT(normalizeLanguageCode("PL") == "pl");
    EXPECT(normalizeLanguageCode("../x").isEmpty());
    EXPECT(languageName("pl") == "Polish");
    EXPECT(languageName("de_AT") == "German (AT)");
    const QStringList labels = pluralFormLabels(pluralFormsFor("pl"), 3);
    EXPECT(labels.size() == 3);
    EXPECT(labels[0] == QStringLiteral("n = 1…"));
    EXPECT(labels[1].startsWith("n = 2, 3, 4, 22"));
    EXPECT(labels[2].startsWith("n = 0, 5, 6, 7"));
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    testRoundTrip();
    testParseErrors();
    testProject();
    testNewSourceFile();
    testScanner();
    testScanFolders();
    testLanguages();
    if (g_failures == 0) std::printf("All localization editor tests passed.\n");
    return g_failures == 0 ? 0 : 1;
}
