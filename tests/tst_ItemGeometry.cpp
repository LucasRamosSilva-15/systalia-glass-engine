#include <QTest>
#include <QSGGeometry>
#include "SystaliaGlassItem.hpp"
#include "SystaliaSurfaceProps.hpp"

class TestItemGeometry : public QObject {
    Q_OBJECT

private slots:
    void testItemPropertiesAndDimensions();
    void testVertexAttributesCompatibility();
};

void TestItemGeometry::testItemPropertiesAndDimensions() {
    SystaliaGlassItem item;
    item.setWidth(800.0);
    item.setHeight(600.0);

    QCOMPARE(item.width(), 800.0);
    QCOMPARE(item.height(), 600.0);

    // Validação da sincronização de props
    SystaliaSurfaceProps props;
    props.windowSize = QVector2D(static_cast<float>(item.width()),
                                 static_cast<float>(item.height()));

    QCOMPARE(props.windowSize.x(), 800.0f);
    QCOMPARE(props.windowSize.y(), 600.0f);
}

void TestItemGeometry::testVertexAttributesCompatibility() {
    const QSGGeometry::AttributeSet &attrSet = QSGGeometry::defaultAttributes_TexturedPoint2D();

    // Validação do stride: 16 bytes (vec2 position + vec2 texCoord)
    QCOMPARE(attrSet.stride, 16);
    QCOMPARE(attrSet.count, 2);

    // Atributo 0: Posição (x, y) -> tupleSize == 2
    QCOMPARE(attrSet.attributes[0].position, 0);
    QCOMPARE(attrSet.attributes[0].tupleSize, 2);
    QCOMPARE(attrSet.attributes[0].type, QSGGeometry::FloatType);

    // Atributo 1: Coordenada de Textura (u, v) -> tupleSize == 2
    QCOMPARE(attrSet.attributes[1].position, 1);
    QCOMPARE(attrSet.attributes[1].tupleSize, 2);
    QCOMPARE(attrSet.attributes[1].type, QSGGeometry::FloatType);
}

QTEST_MAIN(TestItemGeometry)
#include "tst_ItemGeometry.moc"
